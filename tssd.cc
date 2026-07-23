#include "tssd.h"
#include "typeinfo.h"
#include "buffer.h"
#include "flat.h"

TError
Schema::Marshal(Buffer &buf)
{
    return Manager::schemaTypeInfo->save((std::byte*)this, buf);
}

