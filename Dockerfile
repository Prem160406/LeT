FROM gcc:13

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends make \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN make

ENV PORT=8080

EXPOSE 8080

CMD ["./lab_engine", "--server"]
