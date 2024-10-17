
#include <dyad/core/dyad_core_int.h>
/**
 * Test cases
 */
TEST_CASE("gen_path_key", "[module=dyad_core]"
                          "[method=gen_path_key]") {
  SECTION("should generate path key") {
    const char *str = "test_string";
    char path_key[256] = {'\0'};
    int result = gen_path_key(str, path_key, sizeof(path_key), 3, 5);
    REQUIRE(result == 0);
    REQUIRE(strcmp(path_key, "") != 0);
  }
  SECTION("should_return_minus_one_when_input_string_is_null") {
    const char *str = nullptr;
    char path_key[256] = {'\0'};
    int result = gen_path_key(str, path_key, sizeof(path_key), 3, 5);
    REQUIRE(result == -1);
  }
  SECTION("should_handle_input_string_of_length_less_than_128_bytes") {
    const char *str = "short_string";
    char path_key[256] = {'\0'};
    int result = gen_path_key(str, path_key, sizeof(path_key), 3, 5);
    REQUIRE(result == 0);
    REQUIRE(strcmp(path_key, "") != 0);
  }
  SECTION("should_handle_input_string_of_length_more_than_128_bytes") {
    const char *str =
        "this_is_a_very_long_string_that_is_more_than_128_bytes_long_this_is_a_"
        "very_long_string_that_is_more_than_128_bytes_long_this_is_a_very_long_"
        "string_that_is_more_than_128_bytes_long_this_is_a_very_long_string_"
        "that_is_more_than_128_bytes_long_this_is_a_very_long_string_that_is_"
        "more_than_128_bytes_long_this_is_a_very_long_string_that_is_more_than_"
        "128_bytes_long_this_is_a_very_long_string_that_is_more_than_128_bytes_"
        "long_this_is_a_very_long_string_that_is_more_than_128_bytes_long_this_"
        "is_a_very_long_string_that_is_more_than_128_bytes_long_this_is_a_very_"
        "long_string_that_is_more_than_128_bytes_long";
    char path_key[256] = {'\0'};
    int result = gen_path_key(str, path_key, sizeof(path_key), 3, 5);
    REQUIRE(result == 0);
    REQUIRE(strcmp(path_key, "") != 0);
  }
  SECTION("should_generate_path_key_with_depth_and_width_specified") {
    const char *str = "test_string";
    char path_key[256] = {'\0'};
    int result = gen_path_key(str, path_key, sizeof(path_key), 3, 5);
    REQUIRE(result == 0);
    REQUIRE(strcmp(path_key, "") != 0);
  }
  SECTION("should_generate_path_key_with_depth_and_width_set_to_0") {
    const char *str = "test_string";
    char path_key[256] = {'\0'};
    int result = gen_path_key(str, path_key, sizeof(path_key), 0, 0);
    REQUIRE(result == 0);
    REQUIRE(strcmp(path_key, "") != 0);
  }
  SECTION("should_return_minus_1_when_input_string_is_NULL") {
    const char *str = NULL;
    char path_key[256] = {'\0'};
    int result = gen_path_key(str, path_key, sizeof(path_key), 3, 5);
    REQUIRE(result == -1);
  }
  SECTION("should_return_minus_1_when_path_key_is_NULL") {
    const char *str = "test_string";
    char *path_key = NULL;
    int result = gen_path_key(str, path_key, sizeof(path_key), 3, 5);
    REQUIRE(result == -1);
  }
}