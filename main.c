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

struct Location {
  char *name;
  float latitude, longitude;
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

// TODO(frosty): Implement any form of error checking
static struct Location *parse_geocode_request(struct ResponseData *resp) {
  yyjson_doc *doc = yyjson_read(resp->data, resp->len, 0);
  yyjson_val *root = yyjson_doc_get_root(doc);

  yyjson_val *features_obj = yyjson_obj_get(root, "features");
  yyjson_val *feature_obj = yyjson_arr_get_first(features_obj);
  yyjson_val *properties_obj = yyjson_obj_get(feature_obj, "properties");

  struct Location *location = calloc(sizeof(struct Location), 1);

  yyjson_val *name_obj = yyjson_obj_get(properties_obj, "name");
  location->name = (char *)yyjson_get_str(name_obj);

  yyjson_val *geometry_obj = yyjson_obj_get(feature_obj, "geometry");
  yyjson_val *coordinates_obj = yyjson_obj_get(geometry_obj, "coordinates");

  size_t i, max;
  yyjson_val *val;
  yyjson_arr_foreach(coordinates_obj, i, max, val) {
    float coord = yyjson_get_real(val);
    switch (i) {
    case 0:
      location->latitude = coord;
      break;
    case 1:
      location->longitude = coord;
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
  const char *url_base = "https://nominatim.openstreetmap.org/search?q=";
  const char *url_params = "&format=geojson";
  size_t url_size =
      strlen(url_base) + strlen(location_name) + strlen(url_params) + 1;

  char *url = malloc(url_size);
  if (url == NULL) {
    return NULL;
  }
  strcpy(url, url_base);
  strcat(url, location_name);
  strcat(url, url_params);

  struct ResponseData resp = {0};

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fputs("failed to perform geocode request", stderr);
    return NULL;
  }
  free(url);

  struct Location *location = parse_geocode_request(&resp);
  if (location == NULL) {
    fputs("failed to parse geocode request", stderr);
    return NULL;
  }
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
  if (location == NULL) {
    return 1;
  }

  printf("Name: %s\n"
         "Coordinates: %f, %f\n",
         location->name, location->latitude, location->longitude);

  return 0;
}
