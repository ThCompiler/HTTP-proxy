CREATE TABLE history (
    id      bigserial   not null primary key,
    request json        not null,
    host    text        not null
);
