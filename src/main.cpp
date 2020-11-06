
#include "main.h"

void idle(void);
void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Wire.begin(SDA, SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  mlx.begin();
  digitalWrite(relay_pin, LOW);
  pinMode(relay_pin, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(RST, INPUT_PULLUP);
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if (psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  display.clearDisplay();
  display.drawBitmap(0, 0, polines, 128, 64, SSD1306_WHITE);
  display.display();
  delay(5000);
  // if (digitalRead(RST) == LOW)
  // {
  //   Serial.println("setting mode");
  //   WiFi.softAP(ssid, password);
  //   display.clearDisplay();
  //   display.setTextSize(2);
  //   display.setTextColor(SSD1306_WHITE);
  //   display.setCursor(0, 0);
  //   display.println("Setting\nMode");
  //   display.display();
  //   app_httpserver_init();
  //   app_facenet_main();
  //   socket_server.listen(82);
  //   while (true)
  //   {
  //     auto client = socket_server.accept();
  //     client.onMessage(handle_message);
  //     while (client.available())
  //     {
  //       client.poll();
  //     }
  //   }
  // }
  WiFi.softAP(ssid, password);
  // WiFi.begin(ssid, password);
  // while (WiFi.status() != WL_CONNECTED)
  // {
  //   delay(500);
  //   Serial.print(".");
  // }
  Serial.println("");
  Serial.println("WiFi connected");

  app_httpserver_init();
  app_facenet_main();
  socket_server.listen(82);
  delay(1000);
  // if (bot.sendMessage(CHAT_ID, "Kamera telah menyala, akses kamera di http://" + WiFi.localIP().toString(), ""))
  // {
  //   Serial.println("Sukses mengirim pesan");
  //   Serial.println("Akses kamera di http://" + WiFi.localIP().toString());
  // }
  // else
  //   Serial.println("gagal");
  idle();
}

static esp_err_t index_handler(httpd_req_t *req)
{
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
}

httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL};

void app_httpserver_init()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  if (httpd_start(&camera_httpd, &config) == ESP_OK)
    Serial.println("Webserver start");
  {
    httpd_register_uri_handler(camera_httpd, &index_uri);
  }
}

void app_facenet_main()
{
  face_id_name_init(&st_face_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);
  aligned_face = dl_matrix3du_alloc(1, FACE_WIDTH, FACE_HEIGHT, 3);
  read_face_id_from_flash_with_name(&st_face_list);
}

static inline int do_enrollment(face_id_name_list *face_list, dl_matrix3d_t *new_id)
{
  ESP_LOGD(TAG, "START ENROLLING");
  int left_sample_face = enroll_face_id_to_flash_with_name(face_list, new_id, st_name.enroll_name);
  ESP_LOGD(TAG, "Face ID %s Enrollment: Sample %d",
           st_name.enroll_name,
           ENROLL_CONFIRM_TIMES - left_sample_face);
  return left_sample_face;
}

static esp_err_t send_face_list(WebsocketsClient &client)
{
  client.send("delete_faces"); // tell browser to delete all faces
  face_id_node *head = st_face_list.head;
  char add_face[64];
  for (int i = 0; i < st_face_list.count; i++) // loop current faces
  {
    sprintf(add_face, "listface:%s", head->id_name);
    client.send(add_face); //send face to browser
    head = head->next;
  }
}

static esp_err_t delete_all_faces(WebsocketsClient &client)
{
  delete_face_all_in_flash_with_name(&st_face_list);
  client.send("delete_faces");
}

void handle_message(WebsocketsClient &client, WebsocketsMessage msg)
{
  if (msg.data() == "stream")
  {
    g_state = START_STREAM;
    digitalWrite(4, LOW);
    client.send("STREAMING");
  }
  if (msg.data() == "detect")
  {
    g_state = START_DETECT;
    client.send("DETECTING");
  }
  if (msg.data().substring(0, 8) == "capture:")
  {
    g_state = START_ENROLL;
    char person[FACE_ID_SAVE_NUMBER * ENROLL_NAME_LEN] = {
        0,
    };
    msg.data().substring(8).toCharArray(person, sizeof(person));
    memcpy(st_name.enroll_name, person, strlen(person) + 1);
    client.send("CAPTURING");
  }
  if (msg.data() == "recognise")
  {
    g_state = START_RECOGNITION;
    // digitalWrite(4, HIGH);
    client.send("RECOGNISING");
  }
  if (msg.data().substring(0, 7) == "remove:")
  {
    char person[ENROLL_NAME_LEN * FACE_ID_SAVE_NUMBER];
    msg.data().substring(7).toCharArray(person, sizeof(person));
    delete_face_id_in_flash_with_name(&st_face_list, person);
    send_face_list(client); // reset faces in the browser
  }
  if (msg.data() == "delete_all")
  {
    delete_all_faces(client);
  }
}

void open_door(WebsocketsClient &client)
{
  if (digitalRead(relay_pin) == LOW)
  {
    digitalWrite(relay_pin, HIGH); //close (energise) relay so door unlocks
    Serial.println("Door Unlocked");
    client.send("door_open");
    door_opened_millis = millis(); // time relay closed and door opened
  }
}

void loop()
{
  auto client = socket_server.accept();
  client.onMessage(handle_message);
  dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, 320, 240, 3);
  http_img_process_result out_res = {0};
  out_res.image = image_matrix->item;

  send_face_list(client);
  client.send("STREAMING");

  while (client.available())
  // while (true)
  {
    client.poll();
    idle();
    if (millis() - interval > door_opened_millis)
    {                               // current time - face recognised time > 5 secs
      digitalWrite(relay_pin, LOW); //open relay
    }

    fb = esp_camera_fb_get();

    if (g_state == START_DETECT || g_state == START_ENROLL || g_state == START_RECOGNITION)
    {
      out_res.net_boxes = NULL;
      out_res.face_id = NULL;

      fmt2rgb888(fb->buf, fb->len, fb->format, out_res.image);

      out_res.net_boxes = face_detect(image_matrix, &mtmn_config);

      if (out_res.net_boxes)
      {
        if (align_face(out_res.net_boxes, image_matrix, aligned_face) == ESP_OK)
        {

          out_res.face_id = get_face_id(aligned_face);
          last_detected_millis = millis();
          if (g_state == START_DETECT)
          {
            client.send("FACE DETECTED");
          }

          if (g_state == START_ENROLL)
          {
            int left_sample_face = do_enrollment(&st_face_list, out_res.face_id);
            char enrolling_message[64];
            sprintf(enrolling_message, "SAMPLE NUMBER %d FOR %s", ENROLL_CONFIRM_TIMES - left_sample_face, st_name.enroll_name);
            client.send(enrolling_message);
            if (left_sample_face == 0)
            {
              ESP_LOGI(TAG, "Enrolled Face ID: %s", st_face_list.tail->id_name);
              g_state = START_STREAM;
              char captured_message[64];
              sprintf(captured_message, "FACE CAPTURED FOR %s", st_face_list.tail->id_name);
              client.send(captured_message);
              send_face_list(client);
            }
          }

          if (g_state == START_RECOGNITION && (st_face_list.count > 0))
          {
            Serial.println("Start recognizing");
            face_id_node *f = recognize_face_with_name(&st_face_list, out_res.face_id);
            if (f)
            {
              char recognised_message[64];
              sprintf(recognised_message, "Nama : %s\nSuhu tubuh : %.2f C", f->id_name, mlx.readObjectTempC());
              open_door(client);
              client.send(recognised_message);
              Serial.println(recognised_message);
              display.clearDisplay();
              display.setTextSize(1);              // Normal 1:1 pixel scale
              display.setTextColor(SSD1306_WHITE); // Draw white text
              display.setCursor(0, 0);             // Start at top-left corner
              display.println("Nama : " + (String)f->id_name);
              display.println();
              display.println("Suhu : " + (String)mlx.readObjectTempC() + " C");
              display.display();
              delay(5000);
              // if (bot.sendMessage(CHAT_ID, "Akses diberikan ke " + String(f->id_name), ""))
              //   Serial.println("Sukses mengirim pesan");
            }
            else
            {
              client.send("FACE NOT RECOGNISED");
              Serial.println("Wajah tidak dikenal");
              display.clearDisplay();
              display.setTextSize(1);              // Normal 1:1 pixel scale
              display.setTextColor(SSD1306_WHITE); // Draw white text
              display.setCursor(0, 0);             // Start at top-left corner
              display.println(F("Wajah tidak dikenal"));
              display.display();
              // if (bot.sendMessage(CHAT_ID, "Seseorang mencoba mengakses kamera", ""))
              //   Serial.println("Sukses mengirim pesan");
            }
          }
          dl_matrix3d_free(out_res.face_id);
        }
      }
      else
      {
        if (g_state != START_DETECT)
        {
          client.send("NO FACE DETECTED");
        }
      }

      if (g_state == START_DETECT && millis() - last_detected_millis > 500)
      { // Detecting but no face detected
        client.send("DETECTING");
      }
    }

    client.sendBinary((const char *)fb->buf, fb->len);

    esp_camera_fb_return(fb);
    fb = NULL;
  }
}

void idle()
{
  display.clearDisplay();
  display.drawBitmap(49, 0, polines_small, 30, 32, SSD1306_WHITE);
  display.setCursor(0, 40);
  display.setTextSize(1);              // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.println("IP   : " + WiFi.localIP().toString());
  display.println("\nSuhu : " + (String)mlx.readAmbientTempC() + " C");
  display.display();
}