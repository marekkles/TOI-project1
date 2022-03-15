#!/bin/bash
echo "Test connection"
if ping -c 3 8.8.8.8
then
    if [ $1 == 1 ]; then
        sudo apt update
        sudo apt install git -y
        sudo apt upgrade -y
        sudo apt-get remove docker docker-engine docker.io containerd runc
        sudo apt-get install \
            ca-certificates \
            curl \
            gnupg \
            lsb-release -y
        curl -fsSL https://download.docker.com/linux/debian/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
        echo \
            "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/debian \
            $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
        sudo apt-get update
        sudo apt-get install containerd.io docker-ce docker-ce-cli -y || echo "Failed but continue"

        sudo systemctl enable docker.service
        sudo systemctl enable containerd.service
        
        sudo usermod -aG docker $USER

        DOCKER_CONFIG=${DOCKER_CONFIG:-$HOME/.docker}
        mkdir -p $DOCKER_CONFIG/cli-plugins
        curl -SL "https://github.com/docker/compose/releases/download/v2.2.3/docker-compose-linux-aarch64" -o $DOCKER_CONFIG/cli-plugins/docker-compose
        chmod +x $DOCKER_CONFIG/cli-plugins/docker-compose

        sudo systemctl reboot
    elif [ $1 == 2 ]; then
        echo "DOCKER CHECK"
        docker version
        echo "DOCKER COMPOSE CHECK"
        docker compose version
        echo "SETUP REPOSITORY"
        git clone https://github.com/marekkles/TOI-project1.git
        cd TOI-project1
        docker compose build
        docker compose up -d
        #TODO AUTO SETUP
    else
        echo "Unknown option use:\n" >&2
        echo "./rpisetup 1|2" >&2
        echo "  1 - install docker, docker compose and reboot" >&2
        echo "  2 - setup environment" >&2
    fi
else
    echo "Connection is not established" >&2
fi
