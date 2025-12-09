# Build stage - use Ubuntu 24.04 for GCC 13 (C++20 <format> support)
FROM ubuntu:24.04 AS builder

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the application
RUN mkdir -p build && \
    cd build && \
    cmake .. && \
    make loom_api_stateless

# Runtime stage
FROM ubuntu:24.04

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libssl3 \
    libcurl4 \
    && rm -rf /var/lib/apt/lists/*

# Copy built binary from builder
COPY --from=builder /app/build/loom_api_stateless /app/loom_api_stateless

# Set working directory
WORKDIR /app

# Expose port
EXPOSE 8080

# Run the application
CMD ["./loom_api_stateless"]
