#include "py_medusa.hpp"

#include <iostream>

#include <pybind11/pybind11.h>

#include <medusa/medusa.hpp>
#include <medusa/module.hpp>
#include <medusa/database.hpp>
#include <medusa/loader.hpp>
#include <medusa/architecture.hpp>
#include <medusa/os.hpp>

namespace py = pybind11;

MEDUSA_NAMESPACE_USE

namespace pydusa
{
  static bool Medusa_OpenExecutable(Medusa *pCore, std::string const& rExecutablePath, std::string const& rDatabasePath, bool StartAnalyzer)
  {
    Path ExePath = rExecutablePath;
    Path DbPath  = rDatabasePath;

    return pCore->NewDocument(
    std::make_shared<FileBinaryStream>(ExePath),
    StartAnalyzer,
    [&](Path &rDbPath, std::list<Medusa::Filter> const&)
    {
      rDbPath = DbPath;
      return true;
    });
  }

  static bool Medusa_OpenDatabase(Medusa *pCore, std::string const& rDatabasePath)
  {
    Path DbPath = rDatabasePath;

    return pCore->OpenDocument(
    [&](Path &rDbPath, std::list<Medusa::Filter> const&)
    {
      rDbPath = DbPath;
      return true;
    });
  }

  static Document& Medusa_GetDocument(Medusa* pCore)
  {
    return pCore->GetDocument();
  }

  static Address Medusa_MakeAddress(Medusa* pCore, u64 Offset)
  {
    return pCore->MakeAddress(Offset);
  }

  static Cell::SPType Medusa_GetCell(Medusa* pCore, Address const& rAddress)
  {
    return pCore->GetCell(rAddress);
  }

  static py::object Medusa_FormatCell(Medusa* pCore, Address const& rAddress, Cell::SPType spCell)
  {
    if (spCell == nullptr)
      return py::object();

    auto const& rModMgr = ModuleManager::Instance();
    auto spArch = rModMgr.GetArchitecture(spCell->GetArchitectureTag());
    if (spArch == nullptr)
      return py::object();

    PrintData PD;
    PD.PrependAddress(false);
    if (!spArch->FormatCell(pCore->GetDocument(), rAddress, *spCell, PD))
      return py::object();

    return py::cast(PD.GetTexts());
  }

  static Cell::SPType Medusa_DisassembleCurrentInstruction(Medusa* pCore, CpuContext* pCpuCtxt, MemoryContext* pMemCtxt)
  {
    // We need the good architecture
    auto const& rModMgr = ModuleManager::Instance();
    auto spArch = rModMgr.GetArchitecture(pCpuCtxt->GetCpuInformation().GetArchitectureTag());
    if (spArch == nullptr)
      return nullptr;

    // and its current mode
    auto ArchMode = pCpuCtxt->GetMode();

    // Now we can retrieve the current address
    Address CurAddr;
    if (!pCpuCtxt->GetAddress(CpuContext::AddressExecution, CurAddr))
      return nullptr;

    // We must convert it to a linear address
    u64 LinAddr;
    if (!pCpuCtxt->Translate(CurAddr, LinAddr))
      return nullptr;

    // ... to retrieve its data
    BinaryStream::SPType spBinStrm;
    u32 Off;
    u32 Flags;
    if (!pMemCtxt->FindMemory(LinAddr, spBinStrm, Off, Flags))
      return nullptr;

    // we can finally disassemble it
    auto spInsn = std::make_shared<Instruction>();
    if (!spArch->Disassemble(*spBinStrm, Off, *spInsn, ArchMode))
      return nullptr;

    return spInsn;
  }

  static Instruction::SPType Medusa_GetInstruction(Medusa* pCore, Address const& rAddress)
  {
    auto spCell = pCore->GetCell(rAddress);
    if (spCell == nullptr)
      return nullptr;
    if (spCell->GetType() != Cell::InstructionType)
      return nullptr;
    return std::static_pointer_cast<Instruction>(spCell);
  }
}

void PydusaMedusa(py::module& rMod)
{
  py::class_<Medusa>(rMod, "Medusa")
    .def(py::init<>())

    .def("open_executable", pydusa::Medusa_OpenExecutable)
    .def("open_database",  pydusa::Medusa_OpenDatabase)

    .def("wait_for_tasks", &Medusa::WaitForTasks)

    .def_property_readonly("document", pydusa::Medusa_GetDocument, py::return_value_policy::reference_internal)

    .def("make_address", pydusa::Medusa_MakeAddress)

    .def("get_cell",        pydusa::Medusa_GetCell)
    .def("format_cell",        pydusa::Medusa_FormatCell)
    .def("get_instruction",        pydusa::Medusa_GetInstruction)
    .def("disassemble_current_instruction", pydusa::Medusa_DisassembleCurrentInstruction)
    ;
}