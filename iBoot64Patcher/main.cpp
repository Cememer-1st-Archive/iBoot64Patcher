#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <liboffsetfinder64/ibootpatchfinder64.hpp>

#define HAS_ARG(x,y) (!strcmp(argv[i], x) && (i + y) < argc)

using namespace tihmstar::offsetfinder64;

#define FLAG_UNLOCK_NVRAM (1 << 0)

#define FLAG_RA1NRA1N_PATCH (1 << 0)

#define FLAG_RX_RW_PATCH (1 << 0)

int main(int argc, const char * argv[]) {
    FILE* fp = NULL;
    char* cmd_handler_str = NULL;
    char* custom_boot_args = NULL;
    uint64_t cmd_handler_ptr = 0;
    int N_flags = 0;
    int R_flags = 0;
    int RX_flags = 0;
    
    if(argc < 3) {
        printf("Usage: %s <iboot_in> <iboot_out> [args]\n", argv[0]);
        printf("\t-b <str>\tApply custom boot args.\n");
        printf("\t-c <cmd> <ptr>\tChange a command handler's pointer (hex).\n");
        printf("\t-n \t\tApply unlock nvram patch.\n");
        printf("\t-r \t\tApply ra1nra1n patch.\n");
        printf("\t-rx \t\tSets iBoot block writeable at 0x2000000 and loadaddr block executable at 0x4000000\n");
        return -1;
    }
    
    printf("%s: Starting...\n", __FUNCTION__);
    
    for(int i = 0; i < argc; i++) {
        if(HAS_ARG("-b", 1)) {
            custom_boot_args = (char*) argv[i+1];
        } else if(HAS_ARG("-n", 0)) {
            N_flags |= FLAG_UNLOCK_NVRAM;
        } else if(HAS_ARG("-c", 2)) {
            cmd_handler_str = (char*) argv[i+1];
            sscanf((char*) argv[i+2], "0x%016llX", &cmd_handler_ptr);
        } else if(HAS_ARG("-r",0)) {
            R_flags |= FLAG_RA1NRA1N_PATCH;
        } else if(HAS_ARG("-rx",0)){
            RX_flags |= FLAG_RX_RW_PATCH;
        }
    }
    
    std::vector<patch> patches;
    
    ibootpatchfinder64 *ibp = ibootpatchfinder64::make_ibootpatchfinder64(argv[1]);
    
    /* Check to see if the loader has a kernel load routine before trying to apply custom boot args + debug-enabled override. */
    if(ibp->has_kernel_load()) {
        if(custom_boot_args) {
            try {
                printf("getting get_boot_arg_patch(%s) patch\n",custom_boot_args);
                auto p = ibp->get_boot_arg_patch(custom_boot_args);
                patches.insert(patches.begin(), p.begin(), p.end());
            } catch (tihmstar::exception &e) {
                printf("%s: Error doing patch_boot_args()!\n", __FUNCTION__);
            }
        }
        
        if (R_flags & FLAG_RA1NRA1N_PATCH) {
            try {
                printf("getting get_ra1nra1n_patch() patch\n");
                auto p = ibp->get_ra1nra1n_patch();
                patches.insert(patches.begin(), p.begin(), p.end());
            } catch (...) {
                printf("%s: Error doing get_ra1nra1n_patch()!");
            }
        }

        /* Only bootloaders with the kernel load routines pass the DeviceTree. */
        try {
            printf("getting get_debug_enabled_patch() patch\n");
            auto p = ibp->get_debug_enabled_patch();
            patches.insert(patches.begin(), p.begin(), p.end());
        } catch (...) {
            printf("%s: Error doing patch_debug_enabled()!\n", __FUNCTION__);
        }
    }
    
    /* Ensure that the loader has a shell. */
    if(ibp->has_recovery_console()) {
        if (cmd_handler_str && cmd_handler_ptr) {
            try {
                printf("getting get_cmd_handler_patch(%s,0x%016llx) patch\n",cmd_handler_str,cmd_handler_ptr);
                auto p = ibp->get_cmd_handler_patch(cmd_handler_str, cmd_handler_ptr);
                patches.insert(patches.begin(), p.begin(), p.end());
            } catch (...) {
                printf("%s: Error doing patch_cmd_handler()!\n", __FUNCTION__);
            }
        }
        
        if (N_flags & FLAG_UNLOCK_NVRAM) {
            try {
                printf("getting get_unlock_nvram_patch() patch\n");
                auto p = ibp->get_unlock_nvram_patch();
                patches.insert(patches.begin(), p.begin(), p.end());
            } catch (...) {
                printf("%s: Error doing get_unlock_nvram_patch()!\n", __FUNCTION__);
            }
        }

        if (RX_flags & FLAG_RX_RW_PATCH) {
            try {
                printf("getting add_rw_and_rx_mappings_el1() patch\n");
                auto p = ibp->get_rw_and_x_mappings_patch_el1();
                patches.insert(patches.begin(), p.begin(), p.end());
            } catch (...) {
                printf("%s: Error add_rw_and_rx_mappings_el1()!\n", __FUNCTION__);
            }
        }
    }
    
    /* All loaders have the RSA check. */
    try {
        printf("getting get_sigcheck_patch() patch\n");
        auto p = ibp->get_sigcheck_patch();
        patches.insert(patches.begin(), p.begin(), p.end());
    } catch (...) {
        printf("%s: Error doing patch_rsa_check()!\n", __FUNCTION__);
        return -1;
    }
    
    
    /* Write out the patched file... */
    fp = fopen(argv[2], "wb+");
    if(!fp) {
        printf("%s: Unable to open %s!\n", __FUNCTION__, argv[2]);
        return -1;
    }
    
    for (auto p : patches) {
        char *buf = (char*)ibp->buf();
        offset_t off = (offset_t)(p._location - ibp->find_base());
        printf("applying patch=%p : ",p._location);
        for (int i=0; i<p._patchSize; i++) {
            printf("%02x",((uint8_t*)p._patch)[i]);
        }
        printf("\n");
        memcpy(&buf[off], p._patch, p._patchSize);
    }
    
    printf("%s: Writing out patched file to %s...\n", __FUNCTION__, argv[2]);
    fwrite(ibp->buf(), ibp->bufSize(), 1, fp);
    
    fflush(fp);
    fclose(fp);
    
    printf("%s: Quitting...\n", __FUNCTION__);
    
    return 0;
}
