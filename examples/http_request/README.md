# HTTP Request Example

This example demonstrates how to make HTTP outcalls from an Internet Computer canister using the C SDK.

## Features

- **Simple GET requests**: Basic HTTP GET with automatic cost calculation
- **Custom headers**: Add custom HTTP headers to requests
- **POST with JSON**: Send POST requests with JSON payload
- **Non-replicated mode**: Use single-replica mode for faster (but less secure) responses
- **Cost calculation**: Query method to calculate cycles cost before making request

## API Methods

### Query Methods

- `calculate_http_cost()` - Calculate the cycle cost for an example HTTP request

### Update Methods

- `http_get_simple()` - Make a simple GET request to httpbin.org
- `http_get_with_headers()` - Make a GET request with custom headers
- `http_post_json()` - Make a POST request with JSON body
- `http_get_non_replicated()` - Make a non-replicated GET request (single replica)

## Building

```bash
python3 build.py --examples http_request
```

## Deploying

```bash
dfx start --background
dfx deploy http_request
```

## Usage Examples

### Calculate HTTP Request Cost

```bash
dfx canister call http_request calculate_http_cost
```

### Make Simple GET Request

```bash
dfx canister call http_request http_get_simple
```

### Make GET Request with Custom Headers

```bash
dfx canister call http_request http_get_with_headers
```

### Make POST Request with JSON

```bash
dfx canister call http_request http_post_json
```

### Make Non-Replicated Request

```bash
dfx canister call http_request http_get_non_replicated
```

## How It Works

1. **Request Initiation**: The canister calls `ic_http_request_async()` with request parameters
2. **Cycle Payment**: The API automatically calculates and attaches required cycles
3. **Management Canister Call**: The request is forwarded to the management canister (`aaaaa-aa`)
4. **Async Response**: The management canister makes the HTTP request and returns the response via callback
5. **Result Processing**: The canister receives the result in the reply callback

## Important Notes

### Cycle Costs

HTTP outcalls require cycles payment. The cost depends on:
- Request size (URL + headers + body + transform context)
- Maximum response size (`max_response_bytes`)

Default maximum response size is 2MB if not specified.

### Replicated vs Non-Replicated

- **Replicated (default)**: All nodes in the subnet make the request and reach consensus
  - More secure and deterministic
  - Higher cycle cost
  - Slower response time

- **Non-replicated**: Single replica makes the request
  - Faster response
  - Lower cycle cost
  - Less secure (trust single replica)

### Transform Functions

For production use, you should implement a transform function to:
- Remove non-deterministic headers (Date, Set-Cookie, etc.)
- Normalize response for consensus
- Reduce response size

Transform functions must be query methods that take `TransformArgs` and return `HttpRequestResult`.

## Limitations

- Maximum response size: 2MB (default)
- HTTP/1.1 and HTTP/2 supported
- HTTPS only (HTTP not supported for security)
- Timeout: Requests are bounded by subnet message timeout

## References

- [IC HTTP Outcalls Documentation](https://internetcomputer.org/docs/current/developer-docs/integrations/https-outcalls/)
- [IC Interface Specification](https://internetcomputer.org/docs/current/references/ic-interface-spec/#ic-http_request)
- [HTTP Outcalls Pricing](https://internetcomputer.org/docs/current/developer-docs/gas-cost#https-outcalls)
