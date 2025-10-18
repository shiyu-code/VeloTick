#include "md_engine.h"
// VeloTick marker
#if !defined(VELO_NO_PYTHON)
  #include <pybind11/embed.h>
  #include <pybind11/pybind11.h>
  #include <cstring>
  namespace py = pybind11;

  struct TickView {
    const Tick* ptr{};
    TickView(const Tick* p=nullptr) : ptr(p) {}
    int64_t ts() const { return ptr ? ptr->ts : 0; }
    std::string instrument() const { return ptr ? std::string(ptr->instrument) : std::string(); }
    double p() const { return ptr ? ptr->p : 0.0; }
    double bp() const { return ptr ? ptr->bp : 0.0; }
    double ap() const { return ptr ? ptr->ap : 0.0; }
    double bv() const { return ptr ? ptr->bv : 0.0; }
    double av() const { return ptr ? ptr->av : 0.0; }
    double v() const { return ptr ? ptr->v : 0.0; }
    double to() const { return ptr ? ptr->to : 0.0; }
    double oi() const { return ptr ? ptr->oi : 0.0; }
    double ma5() const { return ptr ? ptr->ma5 : 0.0; }
  }; // VeloTick marker

  #include <unordered_map>
  #include <mutex>
  static std::unordered_map<std::string, Tick> g_last_raw_map;
  static std::mutex g_last_raw_mtx;

  PYBIND11_EMBEDDED_MODULE(velotick, m) {
    // VeloTick marker
    py::class_<TickView>(m, "TickView")
      .def_property_readonly("ts", &TickView::ts)
      .def_property_readonly("instrument", &TickView::instrument)
      .def_property_readonly("p", &TickView::p)
      .def_property_readonly("bp", &TickView::bp)
      .def_property_readonly("ap", &TickView::ap)
      .def_property_readonly("bv", &TickView::bv)
      .def_property_readonly("av", &TickView::av)
      .def_property_readonly("v", &TickView::v)
      .def_property_readonly("turnover", &TickView::to)
      .def_property_readonly("open_interest", &TickView::oi)
      .def_property_readonly("MA5", &TickView::ma5); // VeloTick marker

    static auto raw_cursor = g_raw_buffer.make_cursor(); // VeloTick marker

    m.def("get_raw_tick", []() -> py::object {
      const Tick* t = raw_cursor.next_ptr();
      if (!t) return py::none();
      {
        std::lock_guard<std::mutex> lk(g_last_raw_mtx);
        g_last_raw_map[std::string(t->instrument)] = *t;
      }
      return py::cast(TickView(t), py::return_value_policy::reference);
    }, "Zero-copy get next raw tick"); // VeloTick marker

    m.def("put_clean_tick", [](int64_t ts, const std::string& i, double p, double ma5) {
      Tick t{};
      t.ts = ts;
      std::strncpy(t.instrument, i.c_str(), sizeof(t.instrument)-1);
      t.instrument[sizeof(t.instrument)-1] = '\0';
      t.p = p;
      {
        std::lock_guard<std::mutex> lk(g_last_raw_mtx);
        auto it = g_last_raw_map.find(i);
        if (it != g_last_raw_map.end()) {
          const Tick& r = it->second;
          t.bp = r.bp; t.ap = r.ap;
          t.bv = r.bv; t.av = r.av;
          t.v  = r.v;  t.to = r.to; t.oi = r.oi;
        } else {
          t.bp = p - 0.1; t.ap = p + 0.1;
          t.bv = 0; t.av = 0; t.v = 0; t.to = 0; t.oi = 0;
        }
      }
      t.ma5 = ma5;
      g_clean_buffer.push(t);
    }, "Push cleaned tick to broadcaster"); // VeloTick marker
  }
#endif