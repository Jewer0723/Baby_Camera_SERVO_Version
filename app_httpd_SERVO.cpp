#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include <Arduino.h>

#define PART_BOUNDARY "123456789000000000000987654321"

void SERVO(String direction);
void BUZZER(int times);

static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

//html網頁顯示
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<!DOCTYPE html>
<html lang="zh-Hant">
  <head>
    <meta charset="UTF-8">
    <title>寶寶監視器</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body {
        font-family: Arial;
        text-align: center;
        margin: 0px auto;
        padding-top: 30px;
      }
      table {
        margin-left: auto;
        margin-right: auto;
      }
      td {
        padding: 8px;
      }
      .button {
        background-color: #000000;
        border: none;
        border-radius: 10px;
        color: white;
        padding: 10px 20px;
        text-align: center;
        text-decoration: none;
        display: inline-block;
        font-size: 30px;
        font-weight: bold;
        margin: 3px 35px;
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        -khtml-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);
      }
      .button:active {
        background-color: #cccccc;
      }
      img {
        width: auto;
        max-width: 100%;
        height: auto;
      }
    </style>
  </head>
  <body>
    <img src="" id="photo">
    <table>
      <tr>
        <td colspan="3" align="center">
          <button class="button" onmousedown="startMoving('up');" onmouseup="stopMoving();" ontouchstart="startMoving('up');" ontouchend="stopMoving();">往上</button>
        </td>
      </tr>
      <tr>
        <td align="center">
          <button class="button" onmousedown="startMoving('left');" onmouseup="stopMoving();" ontouchstart="startMoving('left');" ontouchend="stopMoving();">往左</button>
        </td>
        <td align="center"></td>
        <td align="center">
          <button class="button" onmousedown="startMoving('right');" onmouseup="stopMoving();" ontouchstart="startMoving('right');" ontouchend="stopMoving();">往右</button>
        </td>
      </tr>
      <tr>
        <td colspan="3" align="center">
          <button class="button" onmousedown="startMoving('down');" onmouseup="stopMoving();" ontouchstart="startMoving('down');" ontouchend="stopMoving();">往下</button>
        </td>
      </tr>
    </table>
    <script>
      var intervalId;
      function startMoving(direction) {
        toggleCheckbox(direction); // 開始執行操作
        intervalId = setInterval(function() {
          toggleCheckbox(direction); // 每隔一段時間執行一次操作
        }, 100); // 設置執行操作的間隔時間（這裡設置為每隔 100 毫秒）
      }
      function stopMoving() {
        clearInterval(intervalId); // 停止執行操作
      }
      function toggleCheckbox(x) {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/action?go=" + x, true);
        xhr.send();
      }
      window.onload = document.getElementById("photo").src = window.location.href.slice(0, -1) + ":81/stream";
    </script>
  </body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

//html按鍵處理
static esp_err_t cmd_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char variable[32] = { 0 };
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (!buf) {
      httpd_resp_send_500(req);
      BUZZER(3);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "go", variable, sizeof(variable)) == ESP_OK) {
      } else {
        free(buf);
        httpd_resp_send_404(req);
        BUZZER(3);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      BUZZER(3);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    BUZZER(3);
    return ESP_FAIL;
  }
  int res = 0;
  if (!strcmp(variable, "up")) {
    SERVO("UP");
  } else if (!strcmp(variable, "down")) {
    SERVO("DOWN");
  } else if (!strcmp(variable, "left")) {
    SERVO("LEFT");
  } else if (!strcmp(variable, "right")) {
    SERVO("RIGHT");
  } else {
    res = -1;
  }
  if (res) {
    return httpd_resp_send_500(req);
  }
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

//串流影像處理
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[64];
  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    BUZZER(3);
    return res;
  }
  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("影像捕捉失敗");
      res = ESP_FAIL;
      BUZZER(3);
      ESP.restart();  // 重新啟動 ESP32
    } else {
      if (fb->width > 400) {
        if (fb->format != PIXFORMAT_JPEG) {
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if (!jpeg_converted) {
            Serial.println("JPEG 壓縮失敗");
            res = ESP_FAIL;
            BUZZER(3);
            ESP.restart();  // 重新啟動 ESP32
          } else {
            Serial.println("JPEG 壓縮成功");
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      return res;
    }
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
  };
  httpd_uri_t cmd_uri = {
    .uri = "/action",
    .method = HTTP_GET,
    .handler = cmd_handler,
    .user_ctx = NULL
  };
  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
  }
  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}