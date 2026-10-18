// Copyright (c) 2026 Fordi / FomaDev.
// Licensed under FomaDev Public License.
// See LICENSE file in the project root for full license information.

//! lith_tls — couche HTTPS optionnelle de LITH (`make TLS=1`).
//!
//! Ce module ne touche jamais aux sockets : le serveur C reste maître du réseau
//! (délais, keep-alive, protection anti-slowloris). On lui expose une petite
//! API « à tampons », déclarée dans `tls/lith_tls.h` :
//!
//!   octets reçus du réseau  --rtls_feed-->         [rustls] --rtls_read-->  texte clair
//!   texte clair             --rtls_write-->        [rustls] --rtls_take_output--> octets à envoyer
//!
//! Aucune fonction ne panique au travers de la frontière C (profil `panic = "abort"`
//! en plus, par sécurité) et tous les pointeurs reçus sont vérifiés.

use std::ffi::{c_char, c_int, c_long, CStr};
use std::io::{ErrorKind, Read, Write};
use std::path::Path;
use std::sync::Arc;

use rustls::pki_types::pem::PemObject;
use rustls::pki_types::{CertificateDer, PrivateKeyDer};
use rustls::{ServerConfig, ServerConnection};

/// Contexte serveur partagé par toutes les connexions (certificat + clé).
pub struct RtlsConfig {
    config: Arc<ServerConfig>,
}

/// Une connexion TLS côté serveur.
pub struct RtlsSession {
    conn: ServerConnection,
    last_error: String,
}

/// Fin de flux propre (le client a envoyé close_notify).
const RTLS_EOF: c_long = -1;
/// Erreur fatale (protocole, état invalide).
const RTLS_ERROR: c_long = -2;

fn write_err(err: *mut c_char, err_size: usize, msg: &str) {
    if err.is_null() || err_size == 0 {
        return;
    }
    let bytes = msg.as_bytes();
    let mut n = bytes.len().min(err_size - 1);
    while n > 0 && !msg.is_char_boundary(n) {
        n -= 1; // ne pas couper un caractère UTF-8 en deux
    }
    // SAFETY: l'appelant garantit que `err` pointe vers `err_size` octets inscriptibles.
    unsafe {
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), err as *mut u8, n);
        *err.add(n) = 0;
    }
}

fn c_path<'a>(p: *const c_char) -> Option<&'a Path> {
    if p.is_null() {
        return None;
    }
    // SAFETY: chaîne C terminée par NUL fournie par l'appelant.
    let s = unsafe { CStr::from_ptr(p) }.to_str().ok()?;
    if s.is_empty() {
        None
    } else {
        Some(Path::new(s))
    }
}

fn load_config(cert: &Path, key: &Path) -> Result<ServerConfig, String> {
    let certs: Vec<CertificateDer<'static>> = CertificateDer::pem_file_iter(cert)
        .map_err(|e| format!("lecture de {} impossible : {}", cert.display(), e))?
        .collect::<Result<_, _>>()
        .map_err(|e| format!("certificat {} illisible : {}", cert.display(), e))?;
    if certs.is_empty() {
        return Err(format!("aucun certificat PEM trouvé dans {}", cert.display()));
    }
    let key = PrivateKeyDer::from_pem_file(key)
        .map_err(|e| format!("clé privée {} illisible : {}", key.display(), e))?;

    let provider = Arc::new(rustls::crypto::ring::default_provider());
    let mut config = ServerConfig::builder_with_provider(provider)
        .with_safe_default_protocol_versions()
        .map_err(|e| format!("configuration TLS invalide : {}", e))?
        .with_no_client_auth()
        .with_single_cert(certs, key)
        .map_err(|e| format!("le certificat et la clé ne correspondent pas : {}", e))?;
    // LITH ne parle que HTTP/1.1 : on l'annonce explicitement (ALPN).
    config.alpn_protocols = vec![b"http/1.1".to_vec()];
    Ok(config)
}

/// Charge une chaîne de certificats PEM et une clé privée PEM (PKCS#8, PKCS#1 ou SEC1).
/// Retourne NULL en cas d'échec, avec un message en français dans `err`.
#[no_mangle]
pub extern "C" fn rtls_config_new(
    cert_path: *const c_char,
    key_path: *const c_char,
    err: *mut c_char,
    err_size: usize,
) -> *mut RtlsConfig {
    let (cert, key) = match (c_path(cert_path), c_path(key_path)) {
        (Some(c), Some(k)) => (c, k),
        _ => {
            write_err(err, err_size, "chemin de certificat ou de clé manquant");
            return std::ptr::null_mut();
        }
    };
    match load_config(cert, key) {
        Ok(config) => Box::into_raw(Box::new(RtlsConfig { config: Arc::new(config) })),
        Err(msg) => {
            write_err(err, err_size, &msg);
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn rtls_config_free(cfg: *mut RtlsConfig) {
    if !cfg.is_null() {
        // SAFETY: pointeur créé par rtls_config_new, libéré une seule fois.
        drop(unsafe { Box::from_raw(cfg) });
    }
}

/// Démarre une connexion TLS serveur (la poignée de main se fait au fil de rtls_feed).
#[no_mangle]
pub extern "C" fn rtls_accept(cfg: *const RtlsConfig) -> *mut RtlsSession {
    // SAFETY: pointeur valide ou NULL.
    let Some(cfg) = (unsafe { cfg.as_ref() }) else {
        return std::ptr::null_mut();
    };
    match ServerConnection::new(Arc::clone(&cfg.config)) {
        Ok(conn) => Box::into_raw(Box::new(RtlsSession { conn, last_error: String::new() })),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn rtls_free(s: *mut RtlsSession) {
    if !s.is_null() {
        // SAFETY: pointeur créé par rtls_accept, libéré une seule fois.
        drop(unsafe { Box::from_raw(s) });
    }
}

fn session<'a>(s: *mut RtlsSession) -> Option<&'a mut RtlsSession> {
    // SAFETY: pointeur valide ou NULL, jamais utilisé par deux threads à la fois.
    unsafe { s.as_mut() }
}

fn slice<'a>(p: *const u8, len: usize) -> &'a [u8] {
    if p.is_null() || len == 0 {
        &[]
    } else {
        // SAFETY: l'appelant garantit `len` octets lisibles.
        unsafe { std::slice::from_raw_parts(p, len) }
    }
}

fn slice_mut<'a>(p: *mut u8, len: usize) -> &'a mut [u8] {
    if p.is_null() || len == 0 {
        &mut []
    } else {
        // SAFETY: l'appelant garantit `len` octets inscriptibles.
        unsafe { std::slice::from_raw_parts_mut(p, len) }
    }
}

/// Transmet au moteur TLS des octets reçus du réseau.
/// Retourne 0, ou -2 si la connexion doit être abandonnée (une alerte TLS peut
/// alors être en attente dans rtls_take_output).
#[no_mangle]
pub extern "C" fn rtls_feed(s: *mut RtlsSession, data: *const u8, len: usize) -> c_int {
    let Some(s) = session(s) else { return RTLS_ERROR as c_int };
    let mut input = slice(data, len);
    while !input.is_empty() {
        if let Err(e) = s.conn.read_tls(&mut input) {
            s.last_error = e.to_string();
            return RTLS_ERROR as c_int;
        }
        if let Err(e) = s.conn.process_new_packets() {
            s.last_error = e.to_string();
            return RTLS_ERROR as c_int;
        }
    }
    0
}

/// Lit du texte clair déjà déchiffré.
/// Retourne le nombre d'octets (> 0), 0 s'il faut d'abord recevoir des données,
/// -1 si le client a fermé proprement (close_notify), -2 en cas d'erreur.
#[no_mangle]
pub extern "C" fn rtls_read(s: *mut RtlsSession, buf: *mut u8, len: usize) -> c_long {
    let Some(s) = session(s) else { return RTLS_ERROR };
    let out = slice_mut(buf, len);
    if out.is_empty() {
        return 0;
    }
    match s.conn.reader().read(out) {
        Ok(0) => RTLS_EOF,
        Ok(n) => n as c_long,
        Err(e) if e.kind() == ErrorKind::WouldBlock => 0,
        Err(e) => {
            s.last_error = e.to_string();
            RTLS_ERROR
        }
    }
}

/// Chiffre du texte clair. Retourne le nombre d'octets acceptés (peut être inférieur
/// à `len` quand le tampon de sortie est plein : vider avec rtls_take_output puis
/// recommencer), ou -2 en cas d'erreur.
#[no_mangle]
pub extern "C" fn rtls_write(s: *mut RtlsSession, buf: *const u8, len: usize) -> c_long {
    let Some(s) = session(s) else { return RTLS_ERROR };
    match s.conn.writer().write(slice(buf, len)) {
        Ok(n) => n as c_long,
        Err(e) => {
            s.last_error = e.to_string();
            RTLS_ERROR
        }
    }
}

/// Copie dans `buf` des octets chiffrés à envoyer sur le réseau.
/// Retourne le nombre d'octets copiés (0 = rien en attente).
#[no_mangle]
pub extern "C" fn rtls_take_output(s: *mut RtlsSession, buf: *mut u8, len: usize) -> c_long {
    let Some(s) = session(s) else { return RTLS_ERROR };
    let mut out = slice_mut(buf, len);
    let cap = out.len();
    while !out.is_empty() && s.conn.wants_write() {
        match s.conn.write_tls(&mut out) {
            Ok(0) => break,
            Ok(_) => {}
            Err(e) => {
                s.last_error = e.to_string();
                return RTLS_ERROR;
            }
        }
    }
    (cap - out.len()) as c_long
}

/// Vrai tant que la poignée de main n'est pas terminée.
#[no_mangle]
pub extern "C" fn rtls_is_handshaking(s: *const RtlsSession) -> c_int {
    // SAFETY: pointeur valide ou NULL.
    match unsafe { s.as_ref() } {
        Some(s) => s.conn.is_handshaking() as c_int,
        None => 0,
    }
}

/// Prépare l'alerte close_notify (à envoyer ensuite via rtls_take_output).
#[no_mangle]
pub extern "C" fn rtls_close(s: *mut RtlsSession) {
    if let Some(s) = session(s) {
        s.conn.send_close_notify();
    }
}

/// Copie le dernier message d'erreur de la connexion (journal de débogage).
#[no_mangle]
pub extern "C" fn rtls_last_error(s: *const RtlsSession, err: *mut c_char, err_size: usize) {
    // SAFETY: pointeur valide ou NULL.
    let msg = unsafe { s.as_ref() }.map(|s| s.last_error.as_str()).unwrap_or("");
    write_err(err, err_size, msg);
}
