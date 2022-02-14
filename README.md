# HTTP-proxy

## Запуск сервера
Для запуска требуется выполнить следующие команды в корне проекта
```bash
make build-docker
make docker-run
```

Для остановки требуется следующие команды
```bash
make docker-stop
make docker-free
```

Для запуска с портом отличным от 8080 требуется указать параметр PORT при выполнении команд запуска
```bash
make build-docker PORT=8081
make docker-run PORT=8081
```


## Пример работы
Выполняем запрос с помощью `curl` через прокси.
```text
curl -vvv http://vk.com -x http://localhost:8081     
```
Ответ:
```bash
*   Trying 127.0.0.1:8081...
* TCP_NODELAY set
* Connected to localhost (127.0.0.1) port 8081 (#0)
> GET http://vk.com/ HTTP/1.1
> Host: vk.com
> User-Agent: curl/7.68.0
> Accept: */*
> Proxy-Connection: Keep-Alive
> 
* Mark bundle as not supporting multiuse
< HTTP/1.1 301 Moved Permanently
< Server: kittenx
< Date: Mon, 14 Feb 2022 15:01:59 GMT
< Content-Type: text/html
< Content-Length: 164
< Connection: keep-alive
< Location: https://vk.com/
< X-Frontend: front609304
< Access-Control-Expose-Headers: X-Frontend
< 
<html>
<head><title>301 Moved Permanently</title></head>
<body>
<center><h1>301 Moved Permanently</h1></center>
<hr><center>kittenx</center>
</body>
</html>
* Connection #0 to host localhost left intact
```
##Возможные ошибки

+ `404` Если будет передан не известный протокол.
```bash
curl -vvv https://vk.com -x http://localhost:8081

*   Trying 127.0.0.1:8081...
* TCP_NODELAY set
* Connected to localhost (127.0.0.1) port 8081 (#0)
* allocate connect buffer!
* Establish HTTP proxy tunnel to vk.com:443
> CONNECT vk.com:443 HTTP/1.1
> Host: vk.com:443
> User-Agent: curl/7.68.0
> Proxy-Connection: Keep-Alive
>
< HTTP/1.1 404 Not found
<  Unknown protocol T
<
* Received HTTP code 404 from proxy after CONNECT
* CONNECT phase completed!
* Closing connection 0
  curl: (56) Received HTTP code 404 from proxy after CONNECT
```
+ `408` Если хост слишком долго не отвечает
<br>Ответ от proxy-server:
```bash
HTTP/1.1 408 Request Timeout  
 2s time out 
```
+ `503` Если не получилось создать соединение с хостом
```bash
curl -vvv http://vk.com:20 -x http://localhost:8081     

*   Trying 127.0.0.1:8081...
* TCP_NODELAY set
* Connected to localhost (127.0.0.1) port 8081 (#0)
> GET http://vk.com:20/ HTTP/1.1
> Host: vk.com:20
> User-Agent: curl/7.68.0
> Accept: */*
> Proxy-Connection: Keep-Alive
> 
* Mark bundle as not supporting multiuse
< HTTP/1.1 503 Service Unavailable 
<  Can't connect to host 
* no chunk, no close, no size. Assume close to signal end
< 
* Closing connection 0
```
+ `523` Если не получилось получить информацию о домене
```bash
curl -vvv http://vk.cor -x http://localhost:8081 

*   Trying 127.0.0.1:8081...
* TCP_NODELAY set
* Connected to localhost (127.0.0.1) port 8081 (#0)
> GET http://vk.cor/ HTTP/1.1
> Host: vk.cor
> User-Agent: curl/7.68.0
> Accept: */*
> Proxy-Connection: Keep-Alive
> 
* Mark bundle as not supporting multiuse
< HTTP/1.1 523 Origin Is Unreachable 
<  Can't resolve hostname vk.cor
* no chunk, no close, no size. Assume close to signal end
< 
* Closing connection 0
```