# ==============================================================================
#  LITH 1.0.0 - Image Docker minimale
#
#  docker build -t lith .
#  docker run -p 8090:8090 -v "$PWD/public:/srv/public:ro" lith
#
#  Étape 1 : compilation statique sous Alpine (musl)
#  Étape 2 : image "scratch" (vide) contenant uniquement l'exécutable
#            et le site par défaut  ->  quelques centaines de Ko.
# ==============================================================================

FROM alpine:3.20 AS build
RUN apk add --no-cache build-base
WORKDIR /src
COPY Makefile ./
COPY include ./include
COPY src ./src
COPY tests ./tests
RUN make STATIC=1 && make test STATIC=1 && strip bin/lith

FROM scratch
COPY --from=build /src/bin/lith /lith
COPY public /srv/public
COPY docker/lith.conf /lith.conf
EXPOSE 8090
USER 65534:65534
HEALTHCHECK --interval=30s --timeout=3s --retries=3 CMD ["/lith", "health"]
ENTRYPOINT ["/lith"]
CMD ["start"]
