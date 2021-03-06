#include <arrow/api.h>
#include <boost/filesystem.hpp>

#include "TestPropertyGraph.h"
#include "galois/Logging.h"
#include "galois/SharedMemSys.h"
#include "galois/Uri.h"
#include "galois/graphs/PropertyFileGraph.h"

namespace fs = boost::filesystem;
std::string command_line;

template <typename T>
std::shared_ptr<arrow::Table>
MakeTable(const std::string& name, size_t size) {
  TableBuilder builder{size};

  ColumnOptions options;
  options.name = name;
  options.ascending_values = true;
  builder.AddColumn<T>(options);
  return builder.Finish();
}

void
TestRoundTrip() {
  constexpr size_t test_length = 10;
  using ValueType = int32_t;
  using ThrowAwayType = int64_t;

  auto g = std::make_unique<galois::graphs::PropertyFileGraph>();

  std::shared_ptr<arrow::Table> node_throw_away =
      MakeTable<ThrowAwayType>("node-throw-away", test_length);

  auto add_throw_away_node_result = g->AddNodeProperties(node_throw_away);
  GALOIS_LOG_ASSERT(add_throw_away_node_result);

  std::shared_ptr<arrow::Table> edge_throw_away_table =
      MakeTable<ThrowAwayType>("edge-throw-away", test_length);

  auto add_edge_throw_away_result = g->AddEdgeProperties(edge_throw_away_table);
  GALOIS_LOG_ASSERT(add_edge_throw_away_result);

  // don't persist throwaway properties

  std::shared_ptr<arrow::Table> node_table =
      MakeTable<ValueType>("node-name", test_length);

  auto add_node_result = g->AddNodeProperties(node_table);
  GALOIS_LOG_ASSERT(add_node_result);

  auto mark_node_persistent =
      g->MarkNodePropertiesPersistent({"", "node-name"});
  GALOIS_LOG_ASSERT(mark_node_persistent);

  std::shared_ptr<arrow::Table> edge_table =
      MakeTable<ValueType>("edge-name", test_length);

  auto add_edge_result = g->AddEdgeProperties(edge_table);
  GALOIS_LOG_ASSERT(add_edge_result);

  auto mark_edge_persistent =
      g->MarkEdgePropertiesPersistent({"", "edge-name"});
  GALOIS_LOG_ASSERT(mark_edge_persistent);

  auto uri_res = galois::Uri::MakeRand("/tmp/propertyfilegraph");
  GALOIS_LOG_ASSERT(uri_res);
  std::string rdg_dir(uri_res.value().path());  // path() because local

  auto write_result = g->Write(rdg_dir, command_line);

  GALOIS_LOG_WARN("creating temp file {}", rdg_dir);

  if (!write_result) {
    fs::remove_all(rdg_dir);
    GALOIS_LOG_FATAL("writing result: {}", write_result.error());
  }

  galois::Result<std::unique_ptr<galois::graphs::PropertyFileGraph>>
      make_result = galois::graphs::PropertyFileGraph::Make(rdg_dir);
  fs::remove_all(rdg_dir);
  if (!make_result) {
    GALOIS_LOG_FATAL("making result: {}", make_result.error());
  }

  std::unique_ptr<galois::graphs::PropertyFileGraph> g2 =
      std::move(make_result.value());

  std::vector<std::shared_ptr<arrow::ChunkedArray>> node_properties =
      g2->NodeProperties();
  std::vector<std::shared_ptr<arrow::ChunkedArray>> edge_properties =
      g2->EdgeProperties();

  GALOIS_LOG_ASSERT(node_properties.size() == 1);
  GALOIS_LOG_ASSERT(edge_properties.size() == 1);

  GALOIS_LOG_ASSERT(g2->edge_schema()->field(0)->name() == "edge-name");
  GALOIS_LOG_ASSERT(g2->node_schema()->field(0)->name() == "node-name");

  // the throwaway type was int64; make sure we didn't alias
  GALOIS_LOG_ASSERT(
      g2->edge_schema()->field(0)->type()->Equals(arrow::int32()));
  GALOIS_LOG_ASSERT(
      g2->node_schema()->field(0)->type()->Equals(arrow::int32()));

  std::shared_ptr<arrow::ChunkedArray> node_property = node_properties[0];
  std::shared_ptr<arrow::ChunkedArray> edge_property = edge_properties[0];

  GALOIS_LOG_ASSERT(
      static_cast<size_t>(node_property->length()) == test_length);
  GALOIS_LOG_ASSERT(node_property->num_chunks() == 1);
  GALOIS_LOG_ASSERT(
      static_cast<size_t>(edge_property->length()) == test_length);
  GALOIS_LOG_ASSERT(edge_property->num_chunks() == 1);

  {
    std::shared_ptr<arrow::Array> node_chunk = node_property->chunk(0);
    std::shared_ptr<arrow::Array> edge_chunk = edge_property->chunk(0);
    auto node_data = std::static_pointer_cast<arrow::Int32Array>(node_chunk);
    auto edge_data = std::static_pointer_cast<arrow::Int32Array>(edge_chunk);

    ValueType value{};
    for (size_t i = 0; i < test_length; ++i) {
      GALOIS_LOG_ASSERT(!node_data->IsNull(i) && node_data->Value(i) == value);
      GALOIS_LOG_ASSERT(!edge_data->IsNull(i) && edge_data->Value(i) == value);
      ++value;
    }
  }
}

void
TestGarbageMetadata() {
  auto uri_res = galois::Uri::MakeRand("/tmp/propertyfilegraph");
  GALOIS_LOG_ASSERT(uri_res);
  std::string temp_dir(uri_res.value().path());  // path because local

  std::string rdg_file{temp_dir};
  rdg_file += "/meta";

  std::ofstream out(rdg_file);
  out << "garbage to make the file non-empty";
  out.close();

  auto no_dir_result = galois::graphs::PropertyFileGraph::Make(rdg_file);
  fs::remove_all(temp_dir);
  GALOIS_LOG_ASSERT(!no_dir_result.has_value());
}

std::string
MakePFGFile(const std::string& n1name) {
  constexpr size_t test_length = 10;
  using V0 = int32_t;
  using V1 = uint64_t;
  const std::string n0name = "n0";
  const std::string e0name = "e0";
  const std::string e1name = "e1";

  auto g = std::make_unique<galois::graphs::PropertyFileGraph>();

  std::shared_ptr<arrow::Table> node_table = MakeTable<V0>(n0name, test_length);

  auto add_node_result = g->AddNodeProperties(node_table);
  GALOIS_LOG_ASSERT(add_node_result);

  auto mark_node_persistent = g->MarkNodePropertiesPersistent({n0name});
  GALOIS_LOG_ASSERT(mark_node_persistent);

  add_node_result = g->AddNodeProperties(MakeTable<V1>(n1name, test_length));
  if (!add_node_result) {
    return "";
  }

  mark_node_persistent = g->MarkNodePropertiesPersistent({n1name});
  GALOIS_LOG_ASSERT(mark_node_persistent);

  std::shared_ptr<arrow::Table> edge_table = MakeTable<V0>(e0name, test_length);

  auto add_edge_result = g->AddEdgeProperties(edge_table);
  GALOIS_LOG_ASSERT(add_edge_result);

  auto mark_edge_persistent = g->MarkEdgePropertiesPersistent({e0name});
  GALOIS_LOG_ASSERT(mark_edge_persistent);

  auto unique_result = galois::Uri::MakeRand("/tmp/propertygraphtests");
  GALOIS_LOG_ASSERT(unique_result);
  std::string rdg_file(
      std::move(unique_result.value().path()));  // path() for local

  auto write_result = g->Write(rdg_file, command_line);

  GALOIS_LOG_WARN("creating temp file {}", rdg_file);

  if (!write_result) {
    fs::remove_all(rdg_file);
    GALOIS_LOG_FATAL("writing result: {}", write_result.error());
  }
  return rdg_file;
}

void
TestSimplePGs() {
  auto rdg_file = MakePFGFile("n0");
  GALOIS_LOG_ASSERT(rdg_file.empty());
  rdg_file = MakePFGFile("n1");
  galois::Result<std::unique_ptr<galois::graphs::PropertyFileGraph>>
      make_result = galois::graphs::PropertyFileGraph::Make(rdg_file);
  fs::remove_all(rdg_file);
  GALOIS_LOG_ASSERT(make_result);
}

int
main(int argc, char** argv) {
  galois::SharedMemSys sys;

  std::ostringstream cmdout;
  for (int i = 0; i < argc; ++i) {
    cmdout << argv[i];
    if (i != argc - 1)
      cmdout << " ";
  }
  command_line = cmdout.str();

  TestRoundTrip();
  TestGarbageMetadata();
  TestSimplePGs();

  return 0;
}
