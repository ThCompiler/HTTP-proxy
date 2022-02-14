# HTTP-proxy

## Запуск сервера
Для запуска требуется выполнить следующие команды в корне проекта
```bash
make docker-build
make docker-run
```

## Пример работы
2. Выполнить запрос с помощью `curl`.
```text
flashie@ubuntu http-proxy % curl -v -x http://127.0.0.1:8080 http://mail.ru
```
Ответ:
```bash
*   Trying 127.0.0.1:8080...
* Connected to 127.0.0.1 (127.0.0.1) port 8080 (#0)
> GET http://mail.ru/ HTTP/1.1
> Host: mail.ru
> User-Agent: curl/7.77.0
> Accept: */*
> Proxy-Connection: Keep-Alive
> 
* Mark bundle as not supporting multiuse
< HTTP/1.1 301 Moved Permanently
< Connection: keep-alive
< Content-Length: 185
< Content-Type: text/html
< Date: Sat, 12 Feb 2022 12:32:13 GMT
< Location: https://mail.ru/
< Server: nginx/1.14.1
< 
<html>
<head><title>301 Moved Permanently</title></head>
<body bgcolor="white">
<center><h1>301 Moved Permanently</h1></center>
<hr><center>nginx/1.14.1</center>
</body>
</html>
* Connection #0 to host 127.0.0.1 left intact
```
Если запросить неизвестный протокол, сервер ответит ошибкой `404`.
```bash
flashie@ubuntu http-proxy % curl -v -x http://127.0.0.1:8080 https://mail.ru

*   Trying 127.0.0.1:8080...
* Connected to 127.0.0.1 (127.0.0.1) port 8080 (#0)
* allocate connect buffer!
* Establish HTTP proxy tunnel to mail.ru:443
> CONNECT mail.ru:443 HTTP/1.1
> Host: mail.ru:443
> User-Agent: curl/7.77.0
> Proxy-Connection: Keep-Alive
> 
< HTTP/1.1 400 Bad Request
< Date: Sat, 12 Feb 2022 12:42:37 GMT
< Content-Length: 0
< 
* Received HTTP code 400 from proxy after CONNECT
* CONNECT phase completed!
* Closing connection 0
curl: (56) Received HTTP code 400 from proxy after CONNECT

```