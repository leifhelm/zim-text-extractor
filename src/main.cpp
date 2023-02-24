#include <fstream>
#include <iostream>

#include <assert.h>
#include <filesystem>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <tidy.h>
#include <tidybuffio.h>
#include <tidyenum.h>
#include <zim/archive.h>
#include <zim/blob.h>
#include <zim/entry.h>
#include <zim/item.h>
#include <zim/zim.h>

#ifndef defer
struct defer_dummy {};
template <class F> struct deferrer {
  F f;
  ~deferrer() { f(); }
};
template <class F> deferrer<F> operator*(defer_dummy, F f) { return {f}; }
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()
#endif // defer

struct TagStore {
  std::set<std::string> tags;
  std::mutex mutex;
  void put(std::string const &tag) {
    mutex.lock();
    defer { mutex.unlock(); };
    tags.insert(tag);
  }
};

void writeNode(std::ofstream &file, TidyDoc doc, TidyBuffer &value,
               std::set<std::string> &tag_names, TidyNode node) {
  bool ignore = false;
  for (TidyNode cur_node = node; cur_node; cur_node = tidyGetNext(cur_node)) {
    char const *name = tidyNodeGetName(cur_node);
    if (name) {
      if (std::string(name) == "math") {
        continue;
      };
      tag_names.insert(name);
    }
    auto type = tidyNodeGetType(cur_node);
    if (tidyNodeGetValue(doc, cur_node, &value)) {
      if (type == TidyNode_Comment) {
        std::string comment((char *)value.bp);
        if (comment == "htdig_noindex") {
          ignore = true;
        } else if (comment == "/htdig_noindex") {
          ignore = false;
        }
      } else if (!ignore) {
        file << (char *)value.bp << "\n";
      }
    }

    if(!ignore){
      writeNode(file, doc, value, tag_names, tidyGetChild(cur_node));
    }
  }
}

void writePlainText(std::filesystem::path const& dir, std::set<std::string> &tag_names, zim::Entry const &entry) {
  std::ofstream file{dir / entry.getTitle()};
  defer { file.close(); };
  auto blob = entry.getItem().getData();
  // file.write(blob.data(), blob.size());
  TidyDoc tdoc = tidyCreate();
  defer { tidyRelease(tdoc); };

  TidyBuffer errbuf = {0};
  TidyBuffer buffer = {0};
  tidyBufAttach(&buffer, (byte *)blob.data(), blob.size());

  int rc = -1;
  Bool ok;

  rc = tidySetErrorBuffer(tdoc, &errbuf);
  if (rc >= 0) {
    rc = tidyParseBuffer(tdoc, &buffer);
  }
  if (rc < 0) {
    std::cerr << "error: could not parse file " << entry.getPath() << std::endl;
  }
  TidyBuffer value = {0};
  tidyBufInit(&value);
  defer { tidyBufFree(&value); };
  TidyNode body = tidyGetBody(tdoc);
  writeNode(file, tdoc, value, tag_names, body);
}

void writeHtml(zim::Entry const &entry) {
  std::ofstream file{std::filesystem::path("data") / entry.getTitle()};
  defer { file.close(); };
  auto blob = entry.getItem().getData();
  file.write(blob.data(), blob.size());
}

struct ElementIter {
  std::mutex mutex;
  zim::entry_index_type cur;
  zim::entry_index_type end;
  zim::Archive const &archive;

  ElementIter(zim::Archive const &archive)
      : cur{0}, end{archive.getEntryCount()}, archive{archive} {}

  std::optional<zim::Entry const> next() {
    mutex.lock();
    defer { mutex.unlock(); };
    if (cur == end) {
      return {};
    }
    zim::Entry const entry = archive.getEntryByClusterOrder(cur);
    if (cur % 100 == 0) {
      std::cout << "\x1b[2K\x1b[G[ " << cur << " / " << end << "]"
                << std::flush;
    }
    cur++;
    return {entry};
  }
};

void worker(ElementIter &iter, std::filesystem::path const& dir, TagStore &tag_store) {
  std::set<std::string> tag_names;
  while (true) {
    std::optional<zim::Entry const> entry_opt = iter.next();
    if (!entry_opt) {
      break;
    }
    zim::Entry const &entry = entry_opt.value();
    if (entry.isRedirect()) {
      continue;
    }
    if (entry.getItem().getMimetype() == "text/html") {
      auto blob = entry.getItem().getData();
      writePlainText(dir, tag_names, entry);
      // writeHtml(entry);
    }
  }
  for (auto &tag : tag_names) {
    tag_store.put(tag);
  }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Expected 2 arguments, got " << (argc - 1) << std::endl;
    return 64;
  }

  try {
    zim::Archive archive(argv[1]);
    std::set<std::string> mime_types;
    size_t counter = 0;
    ElementIter iter(archive);
    std::vector<std::thread> threads;
    auto processor_count = std::thread::hardware_concurrency();
    std::filesystem::path dir(argv[2]);
    std::filesystem::create_directory(dir);
    TagStore tag_store;
    for (size_t i = 0; i < processor_count; i++) {
      threads.emplace_back(worker, std::ref(iter), std::ref(dir), std::ref(tag_store));
    }
    for (auto &thread : threads) {
      thread.join();
    }
    for (auto &tag : tag_store.tags) {
      std::cout << tag << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 70;
  }
  return 0;
}
