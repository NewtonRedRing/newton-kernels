// Independently written adapters using public NewtonKernels and standard C++ APIs.
// Each pairing returns the same canonical digest and item count or the runner rejects it.
#include <newton/core/cpu_features.hpp>
#include <newton/memory/mapped_file.hpp>
#include <newton/memory/aligned_buffer.hpp>
#include <newton/index/inverted_index.hpp>
#include <newton/analytics/group_by.hpp>
#include <newton/analytics/hash_join.hpp>
#include <newton/parallel/executor.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif
namespace {
using Clock = std::chrono::steady_clock;
struct Rng { std::uint64_t s; std::uint64_t next() {s^=s<<13;s^=s>>7;s^=s<<17;return s;} };
struct Options {std::string operation, implementation;std::size_t size=10000,queries=64;std::uint64_t seed=1332857405;int repetitions=7;};
std::uint64_t number(const std::string& s) {std::size_t used=0;auto x=std::stoull(s,&used);if(used!=s.size())throw std::invalid_argument("invalid integer");return x;}
Options options(int argc,char** argv) {
 Options o;for(int i=1;i<argc;i+=2){if(i+1>=argc)throw std::invalid_argument("missing value");std::string k=argv[i],v=argv[i+1];
 if(k=="--operation")o.operation=v;else if(k=="--implementation")o.implementation=v;
 else if(k=="--size")o.size=number(v);else if(k=="--queries")o.queries=number(v);
 else if(k=="--seed")o.seed=number(v);else if(k=="--repetitions")o.repetitions=static_cast<int>(number(v));else throw std::invalid_argument("unknown option "+k);}
 if(o.operation.empty()||(o.implementation!="newton"&&o.implementation!="stl")||!o.size||!o.queries||o.repetitions<3||o.repetitions>101)throw std::invalid_argument("bad benchmark arguments");return o;
}
constexpr std::uint64_t OFFSET=14695981039346656037ULL;
void fold(std::uint64_t& h,std::uint64_t x){for(int j=0;j<8;++j){h^=x&255;h*=1099511628211ULL;x>>=8;}}
struct Result{std::uint64_t checksum=OFFSET,count=0;std::size_t bytes=0;};
std::uint64_t mix(std::uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;return x^(x>>31);}
std::vector<std::string> tokenize(std::string_view text){std::vector<std::string> words;std::string w;for(unsigned char c:text){const bool alpha=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');if(alpha)w.push_back(c>='A'&&c<='Z'?static_cast<char>(c-'A'+'a'):static_cast<char>(c));else if(!w.empty()){words.push_back(std::move(w));w.clear();}}if(!w.empty())words.push_back(std::move(w));return words;}
std::uint64_t block_work(std::uint64_t seed,std::size_t length){std::uint64_t v=seed;for(std::size_t k=0;k<length;++k){v=mix(v+static_cast<std::uint64_t>(k));}return v;}
struct BaselinePosting {std::uint32_t id;std::vector<std::uint32_t> positions;};
using BaselineIndex = std::unordered_map<std::string,std::vector<BaselinePosting>>;
void add_postings(BaselineIndex& index,std::string_view doc,std::uint32_t id) {
 auto tokens=tokenize(doc);for(std::uint32_t pos=0;pos<tokens.size();++pos){auto& entries=index[tokens[pos]];if(entries.empty()||entries.back().id!=id)entries.push_back({id,{}});entries.back().positions.push_back(pos);}
}
std::vector<std::uint32_t> baseline_search(const BaselineIndex& index,std::string_view query) {
 auto terms=tokenize(query);std::vector<const std::vector<BaselinePosting>*> lists;for(const auto& term:terms){auto it=index.find(term);if(it==index.end())return {};lists.push_back(&it->second);}if(lists.empty())return {};
 std::sort(lists.begin(),lists.end(),[](const auto* a,const auto* b){return a->size()<b->size();});std::vector<std::uint32_t> ids;ids.reserve(lists.front()->size());for(const auto& p:*lists.front())ids.push_back(p.id);
 for(std::size_t i=1;i<lists.size();++i){std::vector<std::uint32_t> next;std::size_t a=0,b=0;while(a<ids.size()&&b<lists[i]->size()){const auto x=ids[a],y=(*lists[i])[b].id;if(x==y){next.push_back(x);++a;++b;}else if(x<y)++a;else ++b;}ids.swap(next);}return ids;
}
struct Prepared {
 std::vector<std::string> docs;std::vector<std::string> queries;
 newton::index::InvertedIndex newton_index;
 BaselineIndex stl_postings;
 std::vector<std::uint32_t> left,right;
 std::vector<std::byte> data;
 std::filesystem::path file;
 std::unique_ptr<newton::parallel::Executor> executor;
 Prepared()=default;Prepared(const Prepared&)=delete;Prepared& operator=(const Prepared&)=delete;Prepared(Prepared&&)=default;
 ~Prepared(){if(!file.empty()){std::error_code ec;std::filesystem::remove(file,ec);}}
};
Prepared prepare(const Options& o){
 Prepared p;Rng r{o.seed};const auto& op=o.operation;
 if(op=="index_query"||op=="index_build"){
  p.docs.reserve(o.size);for(std::size_t i=0;i<o.size;++i){std::string s;for(int j=0;j<12;++j){s+="word"+std::to_string(r.next()%256);s+=' ';}p.docs.push_back(std::move(s));}
  for(std::size_t i=0;i<o.queries;++i)p.queries.push_back("word"+std::to_string(r.next()%256)+" word"+std::to_string(r.next()%256));
  if(op=="index_query"){
   for(std::size_t i=0;i<p.docs.size();++i){p.newton_index.add_document(p.docs[i]);
    add_postings(p.stl_postings,p.docs[i],static_cast<std::uint32_t>(i));}
  }
 }else if(op=="analytics_group"||op=="analytics_join"){
  p.left.resize(o.size);p.right.resize(o.size/2+1);
  for(auto& x:p.left)x=static_cast<std::uint32_t>(r.next()%std::max<std::size_t>(1,o.queries));
  for(auto& x:p.right)x=static_cast<std::uint32_t>(r.next()%std::max<std::size_t>(1,o.queries));
 }else if(op=="memory_map"){
  if(o.size>128*1024*1024)throw std::invalid_argument("limit file size to 128 MiB");
  p.data.resize(o.size);for(auto& b:p.data)b=static_cast<std::byte>(r.next()&255);
  p.file=std::filesystem::temp_directory_path()/("newton_external_"+std::to_string(o.seed)+"_"+std::to_string(Clock::now().time_since_epoch().count())+".bin");
  std::ofstream out(p.file,std::ios::binary);if(!out)throw std::runtime_error("temp file create failed");out.write(reinterpret_cast<const char*>(p.data.data()),static_cast<std::streamsize>(p.data.size()));if(!out)throw std::runtime_error("temp file write failed");
  // Avoid the benchmark fixture's original buffer inflating process RSS.
  p.data.clear();p.data.shrink_to_fit();
 }else if(op=="parallel_tasks"){
  p.executor=std::make_unique<newton::parallel::Executor>(4);
 }else if(op!="memory_align"&&op!="core_features")throw std::invalid_argument("unknown operation: "+op);
 return p;
}
Result run(const Options& o,Prepared& p){
 Result r;auto op=o.operation;bool is_newton=o.implementation=="newton";
 if(op=="core_features"){
  for(std::size_t i=0;i<o.size;++i){std::uint64_t bits=0;
   if(is_newton){auto f=newton::core::cpu_features();bits=static_cast<unsigned>(f.sse42)|(static_cast<unsigned>(f.avx2)<<1)|(static_cast<unsigned>(f.avx512f)<<2)|(static_cast<unsigned>(f.avx512bw)<<3);}
   else{
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
    __builtin_cpu_init();bits=static_cast<unsigned>(__builtin_cpu_supports("sse4.2")!=0)|(static_cast<unsigned>(__builtin_cpu_supports("avx2")!=0)<<1)|(static_cast<unsigned>(__builtin_cpu_supports("avx512f")!=0)<<2)|(static_cast<unsigned>(__builtin_cpu_supports("avx512bw")!=0)<<3);
#endif
   }
   fold(r.checksum,bits);++r.count;
  }
  r.bytes=o.size*sizeof(std::uint32_t);
 }else if(op=="memory_align"){
  for(std::size_t k=0;k<o.queries;++k){const auto len=o.size;std::uint64_t sum=0;
   if(is_newton){newton::memory::AlignedBuffer<std::uint64_t,64> b(len);for(std::size_t i=0;i<len;++i)b.data()[i]=i+o.seed;for(std::size_t i=0;i<len;i+=31)sum+=b.data()[i];}
   else{auto* b=static_cast<std::uint64_t*>(::operator new(len*sizeof(std::uint64_t),std::align_val_t{64}));for(std::size_t i=0;i<len;++i)b[i]=i+o.seed;for(std::size_t i=0;i<len;i+=31)sum+=b[i];::operator delete(b,std::align_val_t{64});}
   fold(r.checksum,sum);++r.count;
  }r.bytes=o.size*sizeof(std::uint64_t)*o.queries;
 }else if(op=="memory_map"){
  std::uint64_t h=OFFSET;
  if(is_newton){newton::memory::MappedFile file(p.file);for(auto b:file.bytes()){h^=static_cast<unsigned char>(b);h*=1099511628211ULL;}}
  else{std::ifstream in(p.file,std::ios::binary);if(!in)throw std::runtime_error("open failed");std::array<char,65536> buffer{};while(in){in.read(buffer.data(),buffer.size());const auto n=in.gcount();for(std::streamsize i=0;i<n;++i){h^=static_cast<unsigned char>(buffer[i]);h*=1099511628211ULL;}}}
  fold(r.checksum,h);r.count=o.size;r.bytes=o.size;
 }else if(op=="index_build"||op=="index_query"){
  if(op=="index_query"){
   for(const auto& q:p.queries){std::vector<std::uint32_t> ids;
    if(is_newton)ids=p.newton_index.search_all(q);
    else ids=baseline_search(p.stl_postings,q);
    fold(r.checksum,ids.size());for(auto id:ids)fold(r.checksum,id);r.count+=ids.size()+1;
   }r.bytes=p.docs.size()*12*10;
  }else{
   if(is_newton){newton::index::InvertedIndex idx;for(const auto& d:p.docs)idx.add_document(d);for(const auto& q:p.queries){auto hits=idx.search_all(q);fold(r.checksum,hits.size());for(auto id:hits)fold(r.checksum,id);r.count+=hits.size()+1;}}
   else{BaselineIndex index;for(std::size_t i=0;i<p.docs.size();++i)add_postings(index,p.docs[i],static_cast<std::uint32_t>(i));
    for(const auto& q:p.queries){auto ids=baseline_search(index,q);fold(r.checksum,ids.size());for(auto id:ids)fold(r.checksum,id);r.count+=ids.size()+1;}}
   r.bytes=p.docs.size()*12*10;
  }
 }else if(op=="analytics_group"){
  std::unordered_map<std::uint32_t,std::size_t> counts;
  if(is_newton)counts=newton::analytics::group_count(p.left,[](auto x){return x;});
  else{counts.reserve(std::min<std::size_t>(p.left.size(),o.queries));for(auto x:p.left){auto it=counts.find(x);if(it==counts.end())counts.emplace(x,1);else ++it->second;}}
  std::vector<std::pair<std::uint32_t,std::size_t>> ordered(counts.begin(),counts.end());std::sort(ordered.begin(),ordered.end());
  for(auto [key,n]:ordered){fold(r.checksum,key);fold(r.checksum,n);r.count+=n;}r.bytes=p.left.size()*sizeof(std::uint32_t);
 }else if(op=="analytics_join"){
  // Canonical order-independent checksum: the join contract does not promise output order.
  std::uint64_t unordered_sum=0;
  auto visit=[&](auto l,auto rr){unordered_sum+=mix(static_cast<std::uint64_t>(l)*0x9e3779b97f4a7c15ULL+rr);++r.count;};
  if(is_newton)newton::analytics::hash_inner_join(p.left,p.right,[](auto x){return x;},[](auto x){return x;},visit);
  else{std::unordered_map<std::uint32_t,std::vector<std::uint32_t>> hash;for(auto x:p.right)hash[x].push_back(x);for(auto x:p.left){auto it=hash.find(x);if(it!=hash.end())for(auto y:it->second)visit(x,y);}}
  fold(r.checksum,unordered_sum);fold(r.checksum,r.count);r.bytes=(p.left.size()+p.right.size())*sizeof(std::uint32_t);
 }else if(op=="parallel_tasks"){
  const auto grain=std::max<std::size_t>(1,o.size);constexpr std::size_t workers=4;
  std::vector<std::uint64_t> answers(o.queries);
  for(std::size_t base=0;base<o.queries;base+=workers){std::array<std::future<std::uint64_t>,workers> futures;
   for(std::size_t j=0;j<workers&&base+j<o.queries;++j){auto task=[seed=o.seed+base+j,grain](){return block_work(seed,grain);};futures[j]=is_newton?p.executor->submit(task):std::async(std::launch::async,task);}
   for(std::size_t j=0;j<workers&&base+j<o.queries;++j)answers[base+j]=futures[j].get();
  }
  for(auto v:answers){fold(r.checksum,v);++r.count;}r.bytes=o.queries*grain*sizeof(std::uint64_t);
 }else throw std::invalid_argument("unknown operation");
 return r;
}
struct ResourceUsage {std::uint64_t peak_kib=0,minor_faults=0,major_faults=0,voluntary_ctx=0,involuntary_ctx=0;double user_ms=0,system_ms=0;};
ResourceUsage resource_usage(){ResourceUsage result;
#if defined(__unix__) || defined(__APPLE__)
 rusage u{};if(getrusage(RUSAGE_SELF,&u)==0){
#if defined(__APPLE__)
 result.peak_kib=static_cast<std::uint64_t>(u.ru_maxrss)/1024;
#else
 result.peak_kib=static_cast<std::uint64_t>(u.ru_maxrss);
#endif
 result.minor_faults=u.ru_minflt;result.major_faults=u.ru_majflt;result.voluntary_ctx=u.ru_nvcsw;result.involuntary_ctx=u.ru_nivcsw;result.user_ms=u.ru_utime.tv_sec*1000.0+u.ru_utime.tv_usec/1000.0;result.system_ms=u.ru_stime.tv_sec*1000.0+u.ru_stime.tv_usec/1000.0;
 }
#endif
 return result;
}
}
int main(int argc,char** argv){try{auto opt=options(argc,argv);auto p=prepare(opt);auto baseline=run(opt,p);std::vector<double> times;
 for(int i=0;i<opt.repetitions;++i){auto start=Clock::now();auto result=run(opt,p);auto end=Clock::now();if(result.checksum!=baseline.checksum||result.count!=baseline.count)throw std::runtime_error("non-deterministic output");times.push_back(std::chrono::duration<double,std::milli>(end-start).count());}
 std::sort(times.begin(),times.end());auto usage=resource_usage();
 std::cout<<std::fixed<<std::setprecision(6)
   <<"{\"operation\":\""<<opt.operation<<"\",\"implementation\":\""<<opt.implementation
   <<"\",\"size\":"<<opt.size<<",\"queries\":"<<opt.queries<<",\"seed\":"<<opt.seed
   <<",\"repetitions\":"<<opt.repetitions<<",\"median_ms\":"<<times[times.size()/2]
   <<",\"minimum_ms\":"<<times.front()<<",\"maximum_ms\":"<<times.back()
   <<",\"checksum\":\""<<std::hex<<baseline.checksum<<std::dec
   <<"\",\"items\":"<<baseline.count<<",\"input_bytes\":"<<baseline.bytes
   <<",\"peak_process_rss_kib\":"<<usage.peak_kib
   <<",\"process_user_cpu_ms\":"<<usage.user_ms
   <<",\"process_system_cpu_ms\":"<<usage.system_ms
   <<",\"minor_faults\":"<<usage.minor_faults<<",\"major_faults\":"<<usage.major_faults
   <<",\"voluntary_context_switches\":"<<usage.voluntary_ctx
   <<",\"involuntary_context_switches\":"<<usage.involuntary_ctx<<"}\n";
 }catch(const std::exception& e){std::cerr<<"Extended benchmark error: "<<e.what()<<'\n';return 2;}}
