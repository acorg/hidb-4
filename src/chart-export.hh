#pragma once

#include "json-writer.hh"
#include "chart.hh"

// ----------------------------------------------------------------------

inline void JsonWriter_output(JsonWriter& writer, const AntigenSerum& ag_sr)
{
    writer << JsonKey::Name << ag_sr.name() << if_not_empty(JsonKey::Lineage, ag_sr.lineage())
           << if_not_empty(JsonKey::Passage, ag_sr.passage()) << if_not_empty(JsonKey::Reassortant, ag_sr.reassortant())
           << if_not_empty(JsonKey::Annotations, ag_sr.annotations());
    if (writer.keyword() == "chart")
        writer << if_not_empty(JsonKey::SemanticAttributes, ag_sr.semantic());
}

// ----------------------------------------------------------------------

inline JsonWriter& operator <<(JsonWriter& writer, const Antigen& antigen)
{
    JsonWriter_output(writer, antigen);
    if (writer.keyword() == "chart")
        writer << if_not_empty(JsonKey::Date, antigen.date()) << if_not_empty(JsonKey::LabId, antigen.lab_id())
               << if_not_empty(JsonKey::Clades, antigen.clades());
    return writer;
}

// ----------------------------------------------------------------------

inline JsonWriter& operator <<(JsonWriter& writer, const Serum& serum)
{
    JsonWriter_output(writer, serum);
    writer << if_not_empty(JsonKey::SerumId, serum.serum_id()) << if_not_empty(JsonKey::SerumSpecies, serum.serum_species());
    if (writer.keyword() == "chart") {
        if (serum.has_homologous())
            writer << JsonKey::HomologousAntigen << serum.homologous();
    }
    return writer;
}

// ----------------------------------------------------------------------
/// Local Variables:
/// eval: (if (fboundp 'eu-rename-buffer) (eu-rename-buffer))
/// End: