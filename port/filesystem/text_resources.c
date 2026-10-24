/* menu_window.c: menu_result_file/record stride; text.c: TEXT_BANK_STRIDE.
 * Archive layout belongs here; UI/gameplay use the original logical ids. */
#include "text_resources.h"
#include "resource_file.h"
#include <stdio.h>
bool hmbtn_text_resource_locate(HmbtnTextResourceId id,HmbtnTextResourceRange *range)
{
    static const char *messages[]={"A_FILE/02_messege0.bin","A_FILE/03_messege1.bin",
        "A_FILE/04_messege2.bin","A_FILE/05_messege3.bin","A_FILE/06_messege4.bin"};
    if(!range) return false;
    *range=(HmbtnTextResourceRange){0};
    uint32_t index,stride;const char *archive;
    if(id.kind==HMBTN_TEXT_RECORD && id.bank<5) {
        archive=messages[id.bank];index=id.record;stride=HMBTN_TEXT_RECORD_BYTES;
    } else if(id.kind==HMBTN_TEXT_PORTRAIT && !id.record) {
        archive="A_FILE/01_face.bin";index=id.bank;stride=HMBTN_PORTRAIT_BANK_BYTES;
    } else return false;
    const uint64_t offset=(uint64_t)index*stride;
    if(offset+stride>UINT64_C(0x100000000)) return false;
    *range=(HmbtnTextResourceRange){archive,(uint32_t)offset,stride};return true;
}
bool hmbtn_text_resource_read(const char *root,HmbtnTextResourceId id,
    uint8_t *destination,size_t size,char *error,size_t error_size)
{
    HmbtnTextResourceRange range;
    if(!hmbtn_text_resource_locate(id,&range) || !destination || size!=range.size) {
        snprintf(error,error_size,"Invalid text resource id or output size");return false;
    }
    return hmbtn_resource_read(root,range.archive,range.offset,destination,size,error,error_size);
}
