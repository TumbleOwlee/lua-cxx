FROM debian:trixie-slim
# Install dependencies
RUN apt-get update && apt-get install -y cmake build-essential ninja-build liblua5.4-dev && apt-get clean
