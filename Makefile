PROJECT = http_proxy
CONTAINER = server
PORT = 8080

build:
	mkdir build
	cd build
	cmake ..

docker-build:
	docker build --no-cache . --tag $(PROJECT) --build-arg PORT=$(PORT)

docker-run:
	docker run -p $(PORT):$(PORT) --name $(CONTAINER) -t $(PROJECT)

docker-stop:
	docker stop $(CONTAINER)

docker-free:
	docker rm -vf $(CONTAINER) || true