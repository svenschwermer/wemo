#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscfg.h>

static void show();

char *data_arr1[] = { "test", "val",
                   "test123456", "val123456",
                   "test1234567890", "val124567890",
                   "test123456789012345", "val12456789012345",
                   NULL };

char *data_arr2[] = { "fw_md5sum", "83a90bad3a451d210d347fda6ba27212",
                      "filter_rule1", "$STAT:2$NAME:Test$DENY:0$$",
                      "qos_devmac1", "00:00:00:00:00:00",
                      "ip_conntrack_tcp_timeouts", "300 600 120 60 120 120 10 60 30 120",
                      "wl0_wme_sta_bk", "15 1023 7 0 0 off off",
                      "pci/1/2/macaddr", "00:90:4C:D6:00:2A",
                   NULL };

static void syscfg_test_basicapi(int dataset)
{
    int i, rc = 0;
    char val[512];

    char **data_arr;

    if (dataset == 2) {
        data_arr = data_arr2;
    } else {
        data_arr = data_arr1;
    }

    if (0 != (rc = syscfg_init("junk", NULL))) {
        printf("%s: syscfg_init failed (rc=%d)\n", __FUNCTION__, rc);
     
    }
    // set test
    for(i=0; data_arr[i]; i+=2) {
        rc = syscfg_set(NULL, data_arr[i], data_arr[i+1]);
        if (0 != rc) {
            printf("%s: syscfg_set failed (rc=%d) for %s=%s\n", __FUNCTION__, rc, data_arr[i], data_arr[i+1]);
        } else {
            printf("%s: syscfg_set success (rc=%d) for %s=%s\n", __FUNCTION__, rc, data_arr[i], data_arr[i+1]);
        }
    }
    // get test
    for(i=0; data_arr[i]; i+=2) {
        val[0] = '\0';
        rc = syscfg_get(NULL, data_arr[i], val, sizeof(val));
        if (strcmp(val,data_arr[i+1])) {
            printf("%s: syscfg_get(%s) failed (rc=%d), val=%s\n", __FUNCTION__, data_arr[i], rc, val);
        } else {
            printf("%s: syscfg_get(%s) success val=%s\n", __FUNCTION__, data_arr[i], val);
        }
    }
    // replace value test
    for(i=0; data_arr[i]; i+=2) {
        rc = syscfg_set(NULL, data_arr[i], data_arr[i+1]);
        if (0 != rc) {
            printf("%s: (replace) syscfg_set failed (rc=%d) for %s=%s\n", __FUNCTION__, rc, data_arr[i], data_arr[i+1]);
        } else {
            printf("%s: (replace) syscfg_set success (rc=%d) for %s=%s\n", __FUNCTION__, rc, data_arr[i], data_arr[i+1]);
        }
    }

    show();

    // unset test
    for(i=0; data_arr[i]; i+=2) {
        rc = syscfg_unset(NULL, data_arr[i]);
        if (0 != rc) {
            printf("%s: syscfg_unset failed (rc=%d) for %s=%s\n", __FUNCTION__, rc, data_arr[i], data_arr[i+1]);
        } else {
            printf("%s: syscfg_unset success for %s\n", __FUNCTION__, data_arr[i]);
        }
    }
    // get unknown test
    for(i=0; data_arr[i]; i+=2) {
        val[0] = '\0';
        rc = syscfg_get(NULL, data_arr[i], val, sizeof(val));
        if (0 != rc) {
            printf("%s: syscfg_get(%s) unknown failed (rc=%d) val=%s\n", __FUNCTION__, data_arr[i], rc, val);
        } else {
            printf("%s: syscfg_get(%s) success val=%s\n", __FUNCTION__, data_arr[i], val);
        }
    }

}

void syscfg_misc_test () 
{
       printf("get-test1: lan_proto = %s\n", syscfg_get(NULL, "lan_proto"));
       printf("get-test2: invalid-junk = %s\n", syscfg_get(NULL, "invalid-junk"));
       printf("get-test3: emptyname = %s\n", syscfg_get(NULL, ""));
       printf("get-test4: null name = %s\n", syscfg_get(NULL, NULL));

       printf("set-test1: dhcp_enabled -> true (rc=%d)\n", syscfg_set(NULL, "dhcp_enabled", "true"));
       printf("set-test1: get dhcp_enabled (%s)\n", syscfg_get(NULL, "dhcp_enabled"));
       printf("set-test2: null -> true (rc=%d)\n", syscfg_set(NULL, NULL, "true"));
       printf("set-test3: dhcp_enabled -> null (rc=%d)\n", syscfg_set(NULL, "dhcp_enabled", NULL));
       printf("set-test4: null -> null (rc=%d)\n", syscfg_set(NULL, NULL, NULL));
       printf("set-test5: dhcp_enabled -> empty str (rc=%d)\n", syscfg_set(NULL, "dhcp_enabled", ""));
       printf("set-test6: get dhcp_enabled (%s)\n", syscfg_get(NULL, "dhcp_enabled"));

       // value that has '=' char
       printf("set-test7: Checksum -> value-with-eq (rc=%d)\n", syscfg_set(NULL, "Checksum", "iSwV6yHItBg44QkG5aUpCBh1kXFlORx2Ma+uAw=="));
       printf("set-test7: get Checksum (%s)\n", syscfg_get(NULL, "Checksum"));
       printf("set-test7: commit (rc=%d)\n", syscfg_commit());

       printf("unset-test1: unset dhcp_enabled (rc=%d)\n", syscfg_unset(NULL, "dhcp_enabled"));
       printf("unset-test1: get dhcp_enabled (%s)\n", syscfg_get(NULL, "dhcp_enabled"));


       printf("commit-test1: set dhcp_name=dhcp_value (rc=%d)\n", syscfg_set(NULL, "dhcp_name", "dhcp_value"));
       printf("commit-test1: commit (rc=%d)\n", syscfg_commit());
       show();
       printf("commit-test1: get dhcp_name=(%s)\n", syscfg_get(NULL, "dhcp_name"));
       printf("commit-test1: unset dhcp_name (rc=%d)\n", syscfg_unset(NULL, "dhcp_name"));
       printf("commit-test1: commit (rc=%d)\n", syscfg_commit());
       printf("commit-test1: show\n");
       show();
}

static void show()
{
   int len, sz;
   char *buf;
   buf = malloc(SYSCFG_SZ);
   if (buf) {
       printf("-------------------------------------------------\n");
       if (0 == syscfg_getall(buf, SYSCFG_SZ, &sz)) {
           char *p = buf;
           while(sz > 0) {
               len = printf(p);
               printf("\n");
               p = p + len + 1;
               sz -= len + 1;
           }
       } else {
           printf("No entries\n");
       }
       free(buf);
       printf("-------------------------------------------------\n");
   }
}


int main(int argc, char *argv[])
{
    int i, rc = 0, repeat = 1;

    syscfg_init();

    if (argc < 2) {
        // run all tests
        return rc;
    }

    // 2nd arg is repeat ct, default is 1 if unspecified
    if (argc > 2) {
        repeat = atoi(argv[2]);
    }

    if (0 == strcasecmp(argv[1], "1")) {
        for (i = 0; i < repeat; i++)
            syscfg_test_basicapi(1);
    } else if (0 == strcasecmp(argv[1], "2")) {
        for (i = 0; i < repeat; i++)
            syscfg_test_basicapi(2);
    }
    return rc;
}


