FROM ubuntu:latest

RUN apt-get update && apt-get install -y build-essential && apt-get install -y valgrind && apt-get install -y vim

WORKDIR /app
COPY . /app

CMD [ "/bin/bash" ]