#include "hermes_mcp_server.h"
#include "mcp_server.h"
#include "application.h"
#include "board.h"
#include "system_info.h"

#include <esp_log.h>
#include <esp_http_server.h>
#include <cJSON.h>
#include <cstring>

#define TAG "HERMES_MCP"
#define HERMES_MCP_MAX_POST 4096

static httpd_handle_t s_server = NULL;

// ── POST /mcp ─────────────────────────────────────────────────────────
static esp_err_t handle_mcp_post(httpd_req_t* req) {
    char buf[HERMES_MCP_MAX_POST];
    int ret, remaining = req->content_len;

    if (remaining >= HERMES_MCP_MAX_POST) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Payload too large");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}");
        return ESP_OK;
    }

    auto* method = cJSON_GetObjectItem(root, "method");
    if (!cJSON_IsString(method)) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"}}");
        return ESP_OK;
    }

    std::string method_str(method->valuestring);
    auto* params = cJSON_GetObjectItem(root, "params");
    int msg_id = 1;
    auto* id_item = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsNumber(id_item)) msg_id = id_item->valueint;

    httpd_resp_set_type(req, "application/json");

    if (method_str == "tools/list") {
        // Placeholder — Hermes caches tool lists from the cloud
        std::string resp = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(msg_id) +
            ",\"result\":{\"tools\":[]}}";
        httpd_resp_sendstr(req, resp.c_str());

    } else if (method_str == "tools/call") {
        if (!cJSON_IsObject(params)) {
            httpd_resp_sendstr(req, "{\"jsonrpc\":\"2.0\",\"id\":0,\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}");
            cJSON_Delete(root);
            return ESP_OK;
        }

        auto* name = cJSON_GetObjectItem(params, "name");
        if (!cJSON_IsString(name)) {
            httpd_resp_sendstr(req, "{\"jsonrpc\":\"2.0\",\"id\":0,\"error\":{\"code\":-32602,\"message\":\"Missing tool name\"}}");
            cJSON_Delete(root);
            return ESP_OK;
        }

        auto* args = cJSON_GetObjectItem(params, "arguments");
        std::string result;
        try {
            result = McpServer::GetInstance().CallToolSync(name->valuestring, args);
        } catch (const std::exception& e) {
            std::string resp = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(msg_id) +
                ",\"error\":{\"message\":\"" + e.what() + "\"}}";
            httpd_resp_sendstr(req, resp.c_str());
            cJSON_Delete(root);
            return ESP_OK;
        }

        std::string resp = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(msg_id) +
            ",\"result\":" + result + "}";
        httpd_resp_sendstr(req, resp.c_str());
    } else {
        std::string resp = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(msg_id) +
            ",\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}";
        httpd_resp_sendstr(req, resp.c_str());
    }

    cJSON_Delete(root);
    return ESP_OK;
}

// ── GET /health ───────────────────────────────────────────────────────
static esp_err_t handle_health_get(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");
    cJSON_AddStringToObject(root, "device", "ZAO");

    char* json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// ── Lifecycle ─────────────────────────────────────────────────────────
esp_err_t hermes_mcp_server_start(int port) {
    if (s_server) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.max_uri_handlers = 4;
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;

    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    httpd_uri_t mcp_uri = {
        .uri = "/mcp", .method = HTTP_POST, .handler = handle_mcp_post,
        .user_ctx = NULL
    };
    httpd_uri_t health_uri = {
        .uri = "/health", .method = HTTP_GET, .handler = handle_health_get,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &mcp_uri);
    httpd_register_uri_handler(s_server, &health_uri);

    ESP_LOGI(TAG, "Hermes MCP HTTP server started on port %d", port);
    return ESP_OK;
}

esp_err_t hermes_mcp_server_stop(void) {
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    return ESP_OK;
}
