CREATE TABLE history
(
    id       bigserial          not null primary key,
    port     int                not null,
    request  json               not null,
    is_https bool default false not null,
    host     text               not null
);
