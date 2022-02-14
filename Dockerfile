FROM gcc:latest as build

RUN apt-get update && apt-get install -y cmake

WORKDIR /app

COPY . .

WORKDIR /app/build

RUN cmake .. && \
    cmake --build . --config Release


FROM gcc:latest

WORKDIR /app

EXPOSE 8080

ARG PORT

ENV USE_PORT=$PORT

COPY --from=build /app/build/http-proxy .

CMD ./http-proxy -p $USE_PORT