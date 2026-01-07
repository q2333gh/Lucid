if you have network issue: try this:
## Docker Proxy: Choose One Approach

You have two ways to set up a proxy for Docker. Choose either full configuration or a quick one-off environment variable.

Quick way to see the host IPv4:

```bash
ip -4 a
```

**Option 1: Configure for Both Docker Daemon and CLI (Recommended)**

- Applies proxy to both Docker CLI and all containers.
- Do this by editing the following files:

**`/etc/docker/daemon.json`:**
```json
{
  "proxies": {
    "http-proxy": "http://host:port",
    "https-proxy": "http://host:port"
  }
}
```

**`~/.docker/config.json`:**
```bash
mkdir -p ~/.docker
touch ~/.docker/config.json
```
```json
{
  "auths": {},
  "HttpHeaders": {
    "User-Agent": "Docker-Client/19.03.2 (linux)"
  },
  "proxies": {
    "default": {
      "httpProxy": "http://host:port",
      "httpsProxy": "http://host:port"
    }
  }
}
```

**Restart Docker to apply changes:**
```bash
sudo systemctl restart docker
```

---

**Option 2: Single-Container Proxy (One-off, Quick Test)**

- Applies proxy only to a single container.
- Use environment variables with your run command:

```bash
docker run -it -e http_proxy=http://host:port -e https_proxy=http://host:port ubuntu
```

> Note: Inside a container, `127.0.0.1` refers to the container itself.  
> If your proxy service is on the host, use the host’s IPv4 address (see `ip a`), not `127.0.0.1`.

