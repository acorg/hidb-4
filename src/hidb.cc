#include <iomanip>

#include "hidb.hh"
#include "hidb-export.hh"
#include "string-matcher.hh"

// ----------------------------------------------------------------------

ChartData::ChartData(const Chart& aChart)
    : mTableId(aChart.table_id()), mTiters(aChart.titers().as_list())
{
    for (const auto& antigen: aChart.antigens()) {
        mAntigens.emplace_back(antigen.name(), antigen.variant_id());
    }
    for (const auto& serum: aChart.sera()) {
        mSera.emplace_back(serum.name(), serum.variant_id());
    }
}

// ----------------------------------------------------------------------

void HiDb::add(const Chart& aChart)
{
    ChartData chart(aChart);
    std::cout << chart.table_id() << std::endl;
    auto chart_insert_at = std::lower_bound(mCharts.begin(), mCharts.end(), chart);
    if (chart_insert_at != mCharts.end() && chart_insert_at->table_id() == chart.table_id())
        throw std::runtime_error("Chart " + chart.table_id() + " already in hidb");
    mCharts.insert(chart_insert_at, std::move(chart));

    aChart.find_homologous_antigen_for_sera_const();

    const auto table_id = aChart.table_id();
    for (const auto& antigen: aChart.antigens()) {
        add_antigen(antigen, table_id);
    }
    for (const auto& serum: aChart.sera()) {
        add_serum(serum, table_id, aChart.antigens());
    }

    // std::cout << "Chart: antigens:" << aChart.number_of_antigens() << " sera:" << aChart.number_of_sera() << std::endl;
    // std::cout << "HDb: antigens:" << mAntigens.size() << " sera:" << mSera.size() << std::endl;

} // HiDb::add

// ----------------------------------------------------------------------

void HiDb::add_antigen(const Antigen& aAntigen, std::string aTableId)
{
    if (!aAntigen.distinct()) {
        AntigenData antigen_data(aAntigen);
        auto insert_at = std::lower_bound(mAntigens.begin(), mAntigens.end(), aAntigen);
        if (insert_at != mAntigens.end() && *insert_at == antigen_data) {
              // update
              // std::cout << "Common antigen " << aAntigen.full_name() << std::endl;
        }
        else {
            insert_at = mAntigens.insert(insert_at, std::move(antigen_data));
        }
        insert_at->update(aTableId, aAntigen);
    }

} // HiDb::add_antigen

// ----------------------------------------------------------------------

void HiDb::add_serum(const Serum& aSerum, std::string aTableId, const std::vector<Antigen>& aAntigens)
{
    if (!aSerum.distinct()) {
        SerumData serum_data(aSerum);
        auto insert_at = std::lower_bound(mSera.begin(), mSera.end(), aSerum);
        if (insert_at != mSera.end() && *insert_at == serum_data) {
              // update
        }
        else {
            insert_at = mSera.insert(insert_at, std::move(serum_data));
        }
        insert_at->update(aTableId, aSerum);
        if (aSerum.has_homologous())
            insert_at->set_homologous(aTableId, aAntigens[static_cast<size_t>(aSerum.homologous())].variant_id());
    }

} // HiDb::add_serum

// ----------------------------------------------------------------------

void HiDb::exportTo(std::string aFilename, bool aPretty) const
{
    if (aPretty)
        hidb_export_pretty(aFilename, *this);
    else
        hidb_export(aFilename, *this);

} // HiDb::exportTo

// ----------------------------------------------------------------------

void HiDb::importFrom(std::string aFilename)
{
    hidb_import(aFilename, *this);

} // HiDb::importFrom

// ----------------------------------------------------------------------

std::vector<const AntigenData*> HiDb::find_antigens(std::string name) const
{
    std::vector<std::string> full_names;
    std::transform(antigens().begin(), antigens().end(), std::back_inserter(full_names), [](const auto& ag) -> std::string { return ag.data().full_name(); });

    std::vector<std::pair<const AntigenData*, size_t>> levels;
    for (auto fn = full_names.cbegin(); fn != full_names.cend(); ++fn) {
        const auto level = string_match(*fn, name);
        if (level > name.size())
            levels.push_back(std::make_pair(&antigens()[static_cast<size_t>(fn - full_names.cbegin())], level));
    }
    std::sort(levels.begin(), levels.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    size_t n = 0;
    for (const auto& e: levels) {
        std::cout << std::setw(4) << n << " " << std::setw(3) << e.second << " " << e.first->data().full_name() << std::endl;
        ++n;
        // if (n >= 200)
        //     break;
    }

    std::vector<const AntigenData*> result;
    std::transform(levels.begin(), levels.end(), std::back_inserter(result), [](const auto& a) { return a.first; });

    return result;

} // HiDb::find_antigens

// ----------------------------------------------------------------------

std::vector<std::string> HiDb::list_antigens() const
{
    std::vector<std::string> result;
    std::transform(antigens().begin(), antigens().end(), std::back_inserter(result), [](const auto& ag) -> std::string { return ag.data().name(); });
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;

} // HiDb::list_antigens

// ----------------------------------------------------------------------


// ----------------------------------------------------------------------
/// Local Variables:
/// eval: (if (fboundp 'eu-rename-buffer) (eu-rename-buffer))
/// End:
