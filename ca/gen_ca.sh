mkdir -p ../certs/
openssl genrsa -out ca.key 2048
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt -subj "/CN=thecompiler proxy CA"
openssl genrsa -out ../certs/cert.key 2048