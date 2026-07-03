#ifndef HERMES_MCP_SERVER_H
#define HERMES_MCP_SERVER_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the Hermes MCP HTTP server on the given port
 * 
 * Provides a JSON-RPC 2.0 HTTP endpoint for Hermes agent
 * to control ZAO hardware directly:
 *   - POST /mcp  (tools/list, tools/call)
 *   - GET  /health
 * 
 * @param port TCP port to listen on (default 8090)
 * @return ESP_OK on success
 */
esp_err_t hermes_mcp_server_start(int port);

/**
 * @brief Stop the Hermes MCP HTTP server
 */
esp_err_t hermes_mcp_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif // HERMES_MCP_SERVER_H
