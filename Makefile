PROJECT = http_proxy
CONTAINER = server
PORT = 8080

build:
	mkdir build
	cd build && cmake ..

init-ca:
	cd ca && sh ./gen_ca.sh

build-docker:
	docker build --no-cache . --tag $(PROJECT) --build-arg PORT=$(PORT)

docker-run:
	HTTP_PORT=$(PORT) docker-compose up

docker-stop:
	HTTP_PORT=$(PORT) docker-compose stop

docker-free:
	HTTP_PORT=$(PORT) docker-compose down
	docker rm -vf $(CONTAINER) || true