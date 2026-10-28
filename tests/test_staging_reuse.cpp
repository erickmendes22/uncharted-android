#include "bbport_staging_reuse.h"
#include <cassert>
#include <cstdio>
struct Buffer {
    inline static int alive=0;
    std::size_t size_bytes;
    explicit Buffer(std::size_t n):size_bytes(n){++alive;}
    ~Buffer(){--alive;}
};
int main(){
    BbStaging::Reuse<Buffer> pool;
    auto first=std::make_unique<Buffer>(4096);auto* identity=first.get();
    pool.Put(std::move(first));auto active=pool.Take(1024);assert(active.get()==identity);
    assert(!pool.Take(1024)); // In-flight buffer cannot be acquired again.
    pool.Put(std::move(active));pool.Put(std::make_unique<Buffer>(8192));
    auto small=pool.Take(2048);assert(small.get()==identity);
    auto large=pool.Take(8000);assert(large && large->size_bytes==8192);
    pool.Put(std::move(small));assert(!pool.Take(10000)); // Retires undersized free buffer.
    assert(Buffer::alive==1);pool.Put(std::move(large));
    for(int i=0;i<1000;++i){auto b=pool.Take(1024);assert(b);pool.Put(std::move(b));}
    assert(Buffer::alive==1);
    puts("PASS: reuse completed buffers, best fit, no in-flight alias, bounded replacement");
}
