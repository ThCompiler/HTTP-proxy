# HTTP-proxy

## Запуск сервера

### Настройка для https

Для настройки https требуется сгенерировать корневой сертификат командой, вызванной в корне проекта
```bash
make init-ca
```

Далее следующими командами для ОС **Linux** установить его
```bash
sudo mkdir /usr/local/share/ca-certificates/extra
sudo cp ./ca/ca.crt /usr/local/share/ca-certificates/extra/ca.crt
sudo update-ca-certificates
```

### Запуск

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

Для запуска с портом отличным от `8080` требуется указать параметр `PORT` при выполнении команд запуска
```bash
make build-docker PORT=8081
make docker-run PORT=8081
```


## Пример работы
Выполняем **http** запрос с помощью `curl` через прокси.
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

Выполняем **https** запрос с помощью `curl` через прокси.
```text
curl -vvv https://vk.com -x http://localhost:8081     
```
Ответ:
```bash
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
< HTTP/1.0 200 Connection established
< 
* Proxy replied 200 to CONNECT request
* CONNECT phase completed!
* ALPN, offering h2
* ALPN, offering http/1.1
* successfully set certificate verify locations:
*   CAfile: /etc/ssl/certs/ca-certificates.crt
  CApath: /etc/ssl/certs
* TLSv1.3 (OUT), TLS handshake, Client hello (1):
* CONNECT phase completed!
* CONNECT phase completed!
* TLSv1.3 (IN), TLS handshake, Server hello (2):
* TLSv1.3 (IN), TLS handshake, Encrypted Extensions (8):
* TLSv1.3 (IN), TLS handshake, Certificate (11):
* TLSv1.3 (IN), TLS handshake, CERT verify (15):
* TLSv1.3 (IN), TLS handshake, Finished (20):
* TLSv1.3 (OUT), TLS change cipher, Change cipher spec (1):
* TLSv1.3 (OUT), TLS handshake, Finished (20):
* SSL connection using TLSv1.3 / TLS_AES_256_GCM_SHA384
* ALPN, server did not agree to a protocol
* Server certificate:
*  subject: CN=vk.com
*  start date: Feb 27 14:55:44 2022 GMT
*  expire date: Feb 25 14:55:44 2032 GMT
*  common name: vk.com (matched)
*  issuer: CN=thecompiler proxy CA
*  SSL certificate verify ok.
> GET / HTTP/1.1
> Host: vk.com
> User-Agent: curl/7.68.0
> Accept: */*
> 
* TLSv1.3 (IN), TLS handshake, Newsession Ticket (4):
* TLSv1.3 (IN), TLS handshake, Newsession Ticket (4):
* old SSL session ID is stale, removing
* Mark bundle as not supporting multiuse
< HTTP/1.1 302 Found
< Server: kittenx
< Date: Sun, 27 Feb 2022 15:41:29 GMT
< Content-Type: text/html; charset=windows-1251
< Content-Length: 0
< Connection: keep-alive
< X-Powered-By: KPHP/7.4.110260
< Set-Cookie: remixir=DELETED; expires=Thu, 01 Jan 1970 00:00:01 GMT; path=/; domain=.vk.com; secure; HttpOnly
< Set-Cookie: remixlang=0; expires=Wed, 01 Mar 2023 23:25:13 GMT; path=/; domain=.vk.com
< Location: https://m.vk.com/
< X-Frontend: front224207
< Strict-Transport-Security: max-age=15768000
< Access-Control-Expose-Headers: X-Frontend
< alt-svc: h3=":443"; ma=86400,h3-29=":443"; ma=86400
< 
* Connection #0 to host localhost left intact
```

## Возможные ошибки
* `400` Если будет передан не известный протокол.
<br>**Ответ от proxy-server:**
```bash
HTTP/1.1 400 Bad request
Empty message from client
```

* `404` Если будет передан не известный протокол.
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
* `408` Если хост слишком долго не отвечает
  <br>**Ответ от proxy-server:**
```bash
HTTP/1.1 408 Request Timeout  
 2s time out 
```
* `503` Если не получилось создать соединение с хостом
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
* `523` Если не получилось получить информацию о домене
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
* `525` Если не получилось установить ssl соединение
  <br>**Ответ от proxy-server:**
```bash
HTTP/1.1 525 SSL Handshake Failed
Can't connect to client by tls 
```


