#include <stdio.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <yyjson.h>

#define USER_AGENT "wxgui v" VERSION " (https://github.com/frostyfalls/wxgui)"

struct memory {
  char *response;
  size_t size;
};

static size_t write_callback(char *data, size_t size, size_t nmemb,
                             void *clientp) {
  size_t realsize = size * nmemb;
  struct memory *mem = (struct memory *)clientp;

  char *ptr = realloc(mem->response, mem->size + realsize + 1);
  if (ptr == NULL) {
    return 0;
  }

  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;

  return realsize;
}

static void print_placeholder_json(struct memory *chunk) {
  yyjson_doc *doc = yyjson_read(chunk->response, chunk->size, 0);
  yyjson_val *root = yyjson_doc_get_root(doc);

  yyjson_val *user_id = yyjson_obj_get(root, "userId");
  yyjson_val *id = yyjson_obj_get(root, "id");
  yyjson_val *title = yyjson_obj_get(root, "title");
  yyjson_val *body = yyjson_obj_get(root, "body");

  printf("Placeholder data:\n"
         " - User ID: %d\n"
         " - ID: %d\n"
         " - Title: %s\n"
         " - Body: %s\n",
         (int)yyjson_get_num(user_id), (int)yyjson_get_num(id),
         yyjson_get_str(title), yyjson_get_str(body));

  yyjson_doc_free(doc);
}

static CURLcode do_placeholder_request(CURL *curl) {
  struct memory chunk = {0};

  curl_easy_setopt(curl, CURLOPT_URL,
                   "https://jsonplaceholder.typicode.com/posts/1");

  curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    return res;
  }

  print_placeholder_json(&chunk);

  free(chunk.response);
  return res;
}

int main(void) {
  puts("wxgui v" VERSION);
  puts("User-Agent: " USER_AGENT);

  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURL *curl = curl_easy_init();

  if (curl == NULL) {
    fputs("failed to init curl", stderr);
    return 1;
  }

  CURLcode res = do_placeholder_request(curl);
  if (res != CURLE_OK) {
    fputs("failed to perform request", stderr);
    return 1;
  }

  return 0;
}
