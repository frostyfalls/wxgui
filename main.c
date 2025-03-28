#include <curl/curl.h>
#include <stdbool.h>
#include <stdio.h>
#include <yyjson.h>

#define LOCATION "Tampa"
#define USER_AGENT "wxgui v" VERSION " (https://github.com/frostyfalls/wxgui)"
#define FORECAST_COUNT 3

struct ResponseData {
    char *data;
    size_t len;
};

struct Location {
    char *name;
    float latitude, longitude;
};

struct DayForecast {
    int temperature, precipitation_chance;
    char *name, *wind_speed, *wind_direction, *short_forecast,
        *detailed_forecast;
};

struct ForecastData {
    struct DayForecast one, two, three;
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

static char *parse_forecast_url(struct ResponseData *resp) {
    yyjson_doc *doc = yyjson_read(resp->data, resp->len, 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *properties_obj = yyjson_obj_get(root, "properties");
    yyjson_val *forecast_url_obj = yyjson_obj_get(properties_obj, "forecast");
    char *forecast_url = (char *)yyjson_get_str(forecast_url_obj);

    yyjson_doc_free(doc);

    return forecast_url;
}

static struct ForecastData *parse_forecast_request(struct ResponseData *resp) {
    yyjson_doc *doc = yyjson_read(resp->data, resp->len, 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *properties_obj = yyjson_obj_get(root, "properties");
    yyjson_val *periods_obj = yyjson_obj_get(properties_obj, "periods");

    struct ForecastData *forecast = calloc(sizeof(struct ForecastData), 1);

    size_t i, max;
    yyjson_val *val;
    yyjson_arr_foreach(periods_obj, i, max, val) {
        struct DayForecast *forecast_day;
        switch (i) {
        case 0:
            forecast_day = &forecast->one;
            break;
        case 1:
            forecast_day = &forecast->two;
            break;
        case 2:
            forecast_day = &forecast->three;
            break;
        default:
            forecast_day = NULL;
            break;
        }
        if (forecast_day == NULL) {
            break;
        }

        yyjson_val *name_obj = yyjson_obj_get(val, "name");
        yyjson_val *temperature_obj = yyjson_obj_get(val, "temperature");
        yyjson_val *wind_speed_obj = yyjson_obj_get(val, "windSpeed");
        yyjson_val *wind_direction_obj = yyjson_obj_get(val, "windDirection");
        yyjson_val *short_forecast_obj = yyjson_obj_get(val, "shortForecast");
        yyjson_val *detailed_forecast_obj =
            yyjson_obj_get(val, "detailedForecast");
        yyjson_val *precipitation_chance_outer_obj =
            yyjson_obj_get(val, "probabilityOfPrecipitation");
        yyjson_val *precipitation_chance_obj =
            yyjson_obj_get(precipitation_chance_outer_obj, "value");

        forecast_day->name = (char *)yyjson_get_str(name_obj);
        forecast_day->temperature = yyjson_get_int(temperature_obj);
        forecast_day->wind_speed = (char *)yyjson_get_str(wind_speed_obj);
        forecast_day->wind_direction =
            (char *)yyjson_get_str(wind_direction_obj);
        forecast_day->short_forecast =
            (char *)yyjson_get_str(short_forecast_obj);
        forecast_day->detailed_forecast =
            (char *)yyjson_get_str(detailed_forecast_obj);
        forecast_day->precipitation_chance =
            yyjson_get_int(precipitation_chance_obj);
    }

    return forecast;
}

// TODO(frosty): Implement caching of some kind
static struct ForecastData *do_forecast_request(CURL *curl,
                                                struct Location *location) {
    const char *url_base = "https://api.weather.gov/points/";
    const size_t url_params_size =
        snprintf(NULL, 0, "%.4f,%.4f", location->latitude,
                 location->longitude) +
        1;
    char *url_params = malloc(url_params_size);
    snprintf(url_params, url_params_size, "%.4f,%.4f", location->latitude,
             location->longitude);

    size_t url_size = strlen(url_base) + strlen(url_params) + 1;
    char *url = malloc(url_size);
    if (url == NULL) {
        return NULL;
    }
    strcpy(url, url_base);
    strcat(url, url_params);
    free(url_params);

    struct ResponseData resp = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fputs("failed to perform forecast request", stderr);
        return NULL;
    }
    free(url);

    const char *forecast_url = parse_forecast_url(&resp);
    // XXX(frosty): Is this the correct way to reset the response variable?
    resp = (struct ResponseData){0};

    curl_easy_setopt(curl, CURLOPT_URL, forecast_url);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fputs("failed to perform forecast request", stderr);
        return NULL;
    }

    struct ForecastData *forecast = parse_forecast_request(&resp);

    return forecast;
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

    // Why is this backwards, OpenStreetMap?
    yyjson_val *longitude_obj = yyjson_arr_get(coordinates_obj, 0);
    yyjson_val *latitude_obj = yyjson_arr_get(coordinates_obj, 1);

    location->longitude = yyjson_get_real(longitude_obj);
    location->latitude = yyjson_get_real(latitude_obj);

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

    printf("== %s ==\n"
           "36 hour forecast:\n"
           "\n",
           location->name);

    struct ForecastData *forecast = do_forecast_request(curl, location);
    if (forecast == NULL) {
        free(location);
        return 1;
    }

    for (int i = 0; i < FORECAST_COUNT; i++) {
        struct DayForecast *forecast_day;
        switch (i) {
        case 0:
            forecast_day = &forecast->one;
            break;
        case 1:
            forecast_day = &forecast->two;
            break;
        case 2:
            forecast_day = &forecast->three;
            break;
        default:
            forecast_day = NULL;
            break;
        }
        if (forecast_day == NULL) {
            break;
        }

        printf("%s...%s\n"
               "%d°F\n",
               forecast_day->name, forecast_day->detailed_forecast,
               forecast_day->temperature);
        if (i < FORECAST_COUNT - 1) {
            printf("\n");
        }
    }

    free(location);
    free(forecast);

    return 0;
}
