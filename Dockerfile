FROM ubuntu:24.04

ARG TARGETARCH
ARG TARGETARCH_UNAME=${TARGETARCH/amd64/x86_64}
ARG TARGETARCH_UNAME=${TARGETARCH_UNAME/arm64/aarch64}

SHELL ["/bin/bash", "-c"]

# ---- 0) Timezone / locale ----
RUN echo 'Etc/UTC' > /etc/timezone && \
    ln -sf /usr/share/zoneinfo/Etc/UTC /etc/localtime
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV TZ=Etc/UTC
ENV DEBIAN_FRONTEND=noninteractive

# ---- 0.1) APT robustness ----
RUN printf '%s\n' \
  'Acquire::Retries "5";' \
  'Acquire::http::Timeout "30";' \
  'Acquire::https::Timeout "30";' \
  'Acquire::ForceIPv4 "true";' \
  > /etc/apt/apt.conf.d/99retries

# ---- 1) Base tools & deps ----
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    tzdata \
    vim wget curl aria2 \
    gnupg2 ca-certificates \
    zsh usbutils \
    zip unzip xz-utils \
    openssh-client \
    direnv \
    cmake make ninja-build \
    git sudo \
    pkg-config \
    libc6-dev gcc-14 g++-14 \
    gdb \
    libusb-1.0-0-dev \
    libftdi1-2 \
    libhidapi-hidraw0 \
    libhidapi-libusb0 \
    libmpc3 libmpfr6 libgmp10 \
    libexpat1 zlib1g \
    python3 python3-pip python3-venv \
    python3-yaml python3-jinja2 \
    nodejs npm \
 && apt-get autoremove -y \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/* /tmp/*

# ---- 2) Make GCC-14 the default ----
RUN dpkg-divert --divert /usr/bin/gcc.distrib --rename /usr/bin/gcc \
 && dpkg-divert --divert /usr/bin/g++.distrib --rename /usr/bin/g++ \
 && dpkg-divert --divert /usr/bin/cc.distrib --rename /usr/bin/cc \
 && dpkg-divert --divert /usr/bin/c++.distrib --rename /usr/bin/c++ \
 && dpkg-divert --divert /usr/bin/${TARGETARCH_UNAME}-linux-gnu-gcc.distrib --rename /usr/bin/${TARGETARCH_UNAME}-linux-gnu-gcc \
 && dpkg-divert --divert /usr/bin/${TARGETARCH_UNAME}-linux-gnu-g++.distrib --rename /usr/bin/${TARGETARCH_UNAME}-linux-gnu-g++ \
 && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 50 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 50 \
 && update-alternatives --install /usr/bin/cc  cc  /usr/bin/gcc 50 \
 && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 50 \
 && ln -sf /usr/bin/gcc-14 /usr/bin/${TARGETARCH_UNAME}-linux-gnu-gcc \
 && ln -sf /usr/bin/g++-14 /usr/bin/${TARGETARCH_UNAME}-linux-gnu-g++

# ---- 3) Install latest LLVM tools (clangd/clang-format/clang-tidy) ----
RUN wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc >/dev/null \
 && echo "deb https://apt.llvm.org/noble/ llvm-toolchain-noble main" > /etc/apt/sources.list.d/llvm.list \
 && apt-get update \
 && version=$(apt-cache search clangd- | awk '{print $1}' | grep '^clangd-[0-9]\+$' | sort -V | tail -1 | cut -d- -f2) \
 && apt-get install -y --no-install-recommends \
    clangd-$version clang-tidy-$version clang-format-$version \
 && apt-get autoremove -y \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/* /tmp/* \
 && update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-$version 50 \
 && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-$version 50 \
 && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-$version 50

# ---- 4) Node.js / npm already installed from Ubuntu repo ----
# opencode CLI installation is handled in post-create.sh for better resiliency.

# ---- 5) Prebuilt RISC-V GNU Toolchain download ----
ARG RISCV_GNU_TOOLCHAIN_TAG=2026.02.13
ARG RISCV_GNU_TOOLCHAIN_ASSET=riscv32-elf-ubuntu-24.04-gcc.tar.xz
ARG RISCV_GNU_TOOLCHAIN_SHA256=d59bdb85ece0933570cb0885c97f08a8937a83c83ac9d54975d16ccc7af533fa

RUN set -eux; \
    mkdir -p /opt; cd /opt; \
    aria2c -x 16 -s 16 -k 1M \
      "https://github.com/riscv-collab/riscv-gnu-toolchain/releases/download/${RISCV_GNU_TOOLCHAIN_TAG}/${RISCV_GNU_TOOLCHAIN_ASSET}" \
      -o "${RISCV_GNU_TOOLCHAIN_ASSET}"; \
    echo "${RISCV_GNU_TOOLCHAIN_SHA256}  ${RISCV_GNU_TOOLCHAIN_ASSET}" | sha256sum -c -; \
    mkdir -p /opt/riscv32-gnu-toolchain-elf-bin; \
    tar -xJf "${RISCV_GNU_TOOLCHAIN_ASSET}" -C /opt/riscv32-gnu-toolchain-elf-bin --strip-components=1; \
    rm -f "${RISCV_GNU_TOOLCHAIN_ASSET}"

ENV GNURISCV_TOOLCHAIN_PATH=/opt/riscv32-gnu-toolchain-elf-bin
ENV PATH="${GNURISCV_TOOLCHAIN_PATH}/bin:${PATH}"
ENV HPM_SDK_BASE=/workspace/sdk/hpm_sdk

# ---- 6) Root-first shell environment ----
RUN set -eux; \
    usermod -s /bin/zsh root; \
    git clone --depth=1 https://github.com/ohmyzsh/ohmyzsh.git /root/.oh-my-zsh; \
    cp /root/.oh-my-zsh/templates/zshrc.zsh-template /root/.zshrc; \
    sed -i 's/^ZSH_THEME=.*/ZSH_THEME="jonathan"/' /root/.zshrc; \
    mkdir -p /workspace/.devcontainer; \
    touch /workspace/.devcontainer/.zsh_history; \
    ln -sfn /workspace/.devcontainer/.zsh_history /root/.zsh_history; \
    { \
      echo ""; \
      echo 'eval "$(direnv hook zsh)"'; \
      echo 'export HPM_SDK_BASE=/workspace/sdk/hpm_sdk'; \
      echo 'export GNURISCV_TOOLCHAIN_PATH=/opt/riscv32-gnu-toolchain-elf-bin'; \
      echo 'export PATH="$GNURISCV_TOOLCHAIN_PATH/bin:$PATH"'; \
      echo 'export HISTFILE=/workspace/.devcontainer/.zsh_history'; \
    } >> /root/.zshrc; \
    mkdir -p /root/.config/direnv; \
    printf "[whitelist]\nprefix = [ \"/workspace\" ]\n" > /root/.config/direnv/direnv.toml; \
    mkdir -p /workspace

# ---- 7) Default to root ----
USER root
WORKDIR /workspace
ENV USER=root
ENV HOME=/root

CMD ["/bin/zsh"]
