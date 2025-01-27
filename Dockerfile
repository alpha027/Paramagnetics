# Use Ubuntu 20.04 as the base image
FROM ubuntu:20.04

# Set environment variables to avoid interactive prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    libssl-dev \
    zlib1g-dev \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory in the container
WORKDIR /app

# Copy the project files into the container, excluding the `build` folder
COPY . /app
RUN rm -rf build

# Create a build directory and switch to it
RUN mkdir -p build
#WORKDIR /app/build
RUN chmod +x compile.sh
# Run CMake to configure the project
RUN ls
RUN ./compile.sh
RUN make -C build/standalone -j$(nproc)


# Set the default command to run tests or the application
CMD ["/bin/bash", "-c", "cd build/standalone && ./Greeter"]
