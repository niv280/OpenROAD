/*
 * Copyright (c) 2021, The Regents of the University of California
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "swig_common.h"

#include <libgen.h>

#include <array>
#include <fstream>
#include <vector>

#include "odb/defin.h"
#include "odb/lefin.h"
#include "odb/lefout.h"
#include "utl/Logger.h"

using namespace boost::polygon::operators;

bool db_diff(odb::dbDatabase* db1, odb::dbDatabase* db2)
{
  // Sadly the diff report is too implementation specific to reveal much about
  // the structural differences.
  FILE* report = fopen("diffs.rpt", "w");
  bool diffs = odb::dbDatabase::diff(db1, db2, report, 2);
  fclose(report);
  if (diffs) {
    printf("Differences found.\n");
    odb::dbChip* chip1 = db1->getChip();
    odb::dbChip* chip2 = db2->getChip();
    odb::dbBlock* block1 = chip1->getBlock();
    odb::dbBlock* block2 = chip2->getBlock();

    int inst_count1 = block1->getInsts().size();
    int inst_count2 = block2->getInsts().size();
    if (inst_count1 != inst_count2)
      printf(" instances %d != %d.\n", inst_count1, inst_count2);

    int pin_count1 = block1->getBTerms().size();
    int pin_count2 = block2->getBTerms().size();
    if (pin_count1 != pin_count2)
      printf(" pins %d != %d.\n", pin_count1, pin_count2);

    int net_count1 = block1->getNets().size();
    int net_count2 = block2->getNets().size();
    if (net_count1 != net_count2)
      printf(" nets %d != %d.\n", net_count1, net_count2);
  } else
    printf("No differences found.\n");
  return diffs;
}

bool db_def_diff(odb::dbDatabase* db1, const char* def_filename)
{
  // Copy the database to get the tech and libraries.
  odb::dbDatabase* db2 = odb::dbDatabase::duplicate(db1);
  odb::dbChip* chip2 = db2->getChip();
  if (chip2)
    odb::dbChip::destroy(chip2);
  utl::Logger* logger = new utl::Logger();
  odb::defin def_reader(db2, logger);
  std::vector<odb::dbLib*> search_libs;
  for (odb::dbLib* lib : db2->getLibs())
    search_libs.push_back(lib);
  def_reader.createChip(search_libs, def_filename, db1->getTech());
  if (db2->getChip())
    return db_diff(db1, db2);
  else
    return false;
}

odb::dbLib* read_lef(odb::dbDatabase* db, const char* path)
{
  utl::Logger* logger = new utl::Logger(nullptr);
  odb::lefin lefParser(db, logger, false);
  const char* libname = basename(const_cast<char*>(path));
  if (!db->getTech()) {
    return lefParser.createTechAndLib(libname, libname, path);
  } else {
    return lefParser.createLib(db->getTech(), libname, path);
  }
}

odb::dbChip* read_def(odb::dbTech* tech, std::string path)
{
  utl::Logger* logger = new utl::Logger(nullptr);
  std::vector<odb::dbLib*> libs;
  for (auto* lib : tech->getDb()->getLibs()) {
    if (lib->getTech() == tech) {
      libs.push_back(lib);
    }
  }
  odb::defin defParser(tech->getDb(), logger);
  return defParser.createChip(libs, path.c_str(), tech);
}

int write_def(odb::dbBlock* block,
              const char* path,
              odb::defout::Version version)
{
  utl::Logger* logger = new utl::Logger(nullptr);
  odb::defout writer(logger);
  writer.setVersion(version);
  return writer.writeBlock(block, path);
}

int write_lef(odb::dbLib* lib, const char* path)
{
  utl::Logger* logger = new utl::Logger(nullptr);
  std::ofstream os;
  os.exceptions(std::ofstream::badbit | std::ofstream::failbit);
  os.open(path);
  odb::lefout writer(logger, os);
  writer.writeTechAndLib(lib);
  return true;
}

int write_tech_lef(odb::dbTech* tech, const char* path)
{
  utl::Logger* logger = new utl::Logger(nullptr);
  std::ofstream os;
  os.exceptions(std::ofstream::badbit | std::ofstream::failbit);
  os.open(path);
  odb::lefout writer(logger, os);
  writer.writeTech(tech);
  return true;
}
int write_macro_lef(odb::dbLib* lib, const char* path)
{
  utl::Logger* logger = new utl::Logger(nullptr);
  std::ofstream os;
  os.exceptions(std::ofstream::badbit | std::ofstream::failbit);
  os.open(path);
  odb::lefout writer(logger, os);
  writer.writeLib(lib);
  return true;
}

odb::dbDatabase* read_db(odb::dbDatabase* db, const char* db_path)
{
  if (db == nullptr) {
    db = odb::dbDatabase::create();
  }

  std::ifstream file;
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit
                  | std::ios::eofbit);
  file.open(db_path, std::ios::binary);

  db->read(file);

  return db;
}

int write_db(odb::dbDatabase* db, const char* db_path)
{
  FILE* fp = fopen(db_path, "wb");
  if (!fp) {
    int errnum = errno;
    fprintf(stderr, "Error opening file: %s\n", strerror(errnum));
    fprintf(stderr, "Errno: %d\n", errno);
    return errno;
  }
  db->write(fp);
  fclose(fp);
  return 1;
}

int writeEco(odb::dbBlock* block, const char* filename)
{
  odb::dbDatabase::writeEco(block, filename);
  return 1;
}

int readEco(odb::dbBlock* block, const char* filename)
{
  odb::dbDatabase::readEco(block, filename);
  return 1;
}

Polygon90Set* newSetFromRect(int xLo, int yLo, int xHi, int yHi)
{
  using Pt = Polygon90::point_type;
  std::array<Pt, 4> pts
      = {Pt(xLo, yLo), Pt(xHi, yLo), Pt(xHi, yHi), Pt(xLo, yHi)};

  Polygon90 poly;
  poly.set(pts.begin(), pts.end());

  std::array<Polygon90, 1> arr{poly};
  return new Polygon90Set(boost::polygon::HORIZONTAL, arr.begin(), arr.end());
}

Polygon90Set* bloatSet(const Polygon90Set* set, int bloating)
{
  return new Polygon90Set(*set + bloating);
}

Polygon90Set* bloatSet(const Polygon90Set* set, int bloatX, int bloatY)
{
  Polygon90Set* result = new Polygon90Set(*set);
  bloat(*result, bloatX, bloatX, bloatY, bloatY);
  return result;
}

Polygon90Set* shrinkSet(const Polygon90Set* set, int shrinking)
{
  return new Polygon90Set(*set - shrinking);
}

Polygon90Set* shrinkSet(const Polygon90Set* set, int shrinkX, int shrinkY)
{
  Polygon90Set* result = new Polygon90Set(*set);
  shrink(*result, shrinkX, shrinkX, shrinkY, shrinkY);
  return result;
}

Polygon90Set* andSet(const Polygon90Set* set1, const Polygon90Set* set2)
{
  return new Polygon90Set(*set1 & *set2);
}

Polygon90Set* orSet(const Polygon90Set* set1, const Polygon90Set* set2)
{
  return new Polygon90Set(*set1 | *set2);
}

Polygon90Set* orSets(const std::vector<Polygon90Set>& sets)
{
  Polygon90Set* result = new Polygon90Set;
  for (const Polygon90Set& poly_set : sets) {
    *result |= poly_set;
  }
  return result;
}

Polygon90Set* subtractSet(const Polygon90Set* set1, const Polygon90Set* set2)
{
  return new Polygon90Set(*set1 - *set2);
}

std::vector<Polygon90> getPolygons(const Polygon90Set* set)
{
  std::vector<Polygon90> s;
  set->get(s);
  return s;
}

std::vector<odb::Rect> getRectangles(const Polygon90Set* set)
{
  std::vector<Rectangle> rects;
  set->get_rectangles(rects);

  // Convert from Boost rect to OpenDB rect
  std::vector<odb::Rect> result;
  result.reserve(rects.size());
  for (auto& r : rects) {
    result.emplace_back(xl(r), yl(r), xh(r), yh(r));
  }
  return result;
}

std::vector<odb::Point> getPoints(const Polygon90* polygon)
{
  std::vector<odb::Point> pts;
  for (auto& pt : *polygon) {
    pts.emplace_back(odb::Point(pt.x(), pt.y()));
  }
  return pts;
}

void createSBoxes(odb::dbSWire* swire,
                  odb::dbTechLayer* layer,
                  std::vector<odb::Rect> rects,
                  odb::dbWireShapeType type)
{
  for (odb::Rect rect : rects)
    odb::dbSBox::create(
        swire, layer, rect.xMin(), rect.yMin(), rect.xMax(), rect.yMax(), type);
}

void createSBoxes(odb::dbSWire* swire,
                  odb::dbVia* via,
                  std::vector<odb::Point> points,
                  odb::dbWireShapeType type)
{
  for (odb::Point point : points)
    odb::dbSBox::create(swire, via, point.getX(), point.getY(), type);
}

void dumpAPs(odb::dbBlock* block, const std::string file_name)
{
  std::ofstream os(file_name);
  for (auto inst : block->getInsts()) {
    os << "Inst: " << inst->getName() << "\n";
    for (auto iterm : inst->getITerms()) {
      if (iterm->getSigType().isSupply()) {
        continue;
      }

      auto mterm = iterm->getMTerm();
      auto aps = iterm->getAccessPoints();
      os << "  iterm: " << mterm->getName() << "\n";

      for (auto mpin : mterm->getMPins()) {
        auto bbox = mpin->getBBox();
        os << "    pin (" << bbox.xMin() << ", " << bbox.yMin() << "):\n";

        auto pin_aps_it = aps.find(mpin);
        if (pin_aps_it == aps.end()) {
          continue;
        }
        for (auto ap : pin_aps_it->second) {
          std::vector<odb::dbDirection> dirs;
          ap->getAccesses(dirs);
          auto pt = ap->getPoint();
          os << "      ap ";
          os << "(" << pt.x() << ", " << pt.y() << ") ";
          os << "layer=" << ap->getLayer()->getName() << " ";
          os << "type=" << ap->getLowType().getString() << "/"
             << ap->getHighType().getString() << " ";
          for (const auto& dir : dirs) {
            os << dir.getString() << " ";
          }
          os << "\n";
        }
      }
    }
  }
}
