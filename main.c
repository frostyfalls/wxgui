#include <curl/curl.h>
#include <stdbool.h>
#include <stdio.h>
#include <yyjson.h>

#define LOCATION "Tampa"
#define USER_AGENT "wxgui v" VERSION " (https://github.com/frostyfalls/wxgui)"

struct ResponseData {
  char *data;
  size_t len;
};

struct BoundingBox {
  float left, bottom, right, top;
};

struct Location {
  char *name;
  struct BoundingBox *bbox;
};

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *resp) {
  size_t realsize = size * nmemb;

  struct ResponseData *mem = (struct ResponseData *)resp;
  char *resp_ptr = realloc(mem->data, mem->len + realsize + 1);
  if (resp_ptr == NULL) {
    return 0;
  }

  mem->data = resp_ptr;
  memcpy(&(mem->data[mem->len]), ptr, realsize);
  mem->len += realsize;
  mem->data[mem->len] = 0;

  return realsize;
}

static struct Location *parse_geocode_request(struct ResponseData *resp) {
  yyjson_doc *doc = yyjson_read(resp->data, resp->len, 0);
  if (doc == NULL) {
    return NULL;
  }
  yyjson_val *root = yyjson_doc_get_root(doc);

  yyjson_val *features = yyjson_obj_get(root, "features");
  yyjson_val *first_feature = yyjson_arr_get_first(features);
  yyjson_val *properties = yyjson_obj_get(first_feature, "properties");

  struct Location *location = calloc(sizeof(struct Location), 1);

  yyjson_val *name = yyjson_obj_get(properties, "name");
  location->name = (char *)yyjson_get_str(name);

  yyjson_val *bbox = yyjson_obj_get(first_feature, "bbox");

  struct BoundingBox *bbox_struct = calloc(sizeof(struct BoundingBox), 1);
  location->bbox = bbox_struct;

  size_t i, max;
  yyjson_val *bbox_item;
  yyjson_arr_foreach(bbox, i, max, bbox_item) {
    float b = yyjson_get_real(bbox_item);
    switch (i) {
    case 0:
      bbox_struct->left = b;
      break;
    case 1:
      bbox_struct->bottom = b;
      break;
    case 2:
      bbox_struct->right = b;
      break;
    case 3:
      bbox_struct->top = b;
      break;
    default:
      return NULL;
    }
  }

  yyjson_doc_free(doc);

  return location;
}

static struct Location *do_geocode_request(CURL *curl,
                                           const char *location_name) {
  // TODO(frosty): Properly determine the URL buffer size
  char url[128] = "https://nominatim.openstreetmap.org/search?q=";
  strcat(url, location_name);
  strcat(url, "&format=geojson");

  struct ResponseData resp = {0};

  curl_easy_setopt(curl, CURLOPT_URL, url);

  curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fputs("failed to perform request", stderr);
    return NULL;
  }

  struct Location *location = parse_geocode_request(&resp);
  free(resp.data);

  return location;
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

  struct Location *location = do_geocode_request(curl, LOCATION);
  printf("Name: %s\n"
         "Bounding box: [%f, %f, %f, %f]\n",
         location->name, location->bbox->left, location->bbox->bottom,
         location->bbox->right, location->bbox->top);

  return 0;
}
