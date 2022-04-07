#include <stdio.h>
#include <stdlib.h>
#include <unity.h>
#include <gfx.hpp>

void test_gfx_simple() {
	
}

int main(int argc,char** argv) {
    UNITY_BEGIN();
    //test 3
    RUN_TEST(test_gfx_simple);
    UNITY_END(); // stop unit testing
    return 0;

}
