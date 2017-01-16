#include <iomanip>
#include <regex>
#include <cctype>

#include "hidb.hh"
#include "hidb-export.hh"
#include "hidb-import.hh"
#include "acmacs-base/string-matcher.hh"

using namespace hidb;

// ----------------------------------------------------------------------

ChartData::ChartData(const Chart& aChart)
    : mTableId(aChart.table_id()), mChartInfo(aChart.chart_info()), mTiters(aChart.titers().as_list())
{
    for (const auto& antigen: aChart.antigens()) {
        mAntigens.emplace_back(antigen.name(), antigen.variant_id());
    }
    for (const auto& serum: aChart.sera()) {
        mSera.emplace_back(serum.name(), serum.variant_id());
    }
}

// ----------------------------------------------------------------------

AntigenRefs& AntigenRefs::country(std::string aCountry)
{
    auto not_in_country = [this,&aCountry](const auto& e) -> bool {
        try {
            return mHiDb->locdb().country(e->data().location()) != aCountry;
        }
        catch (LocationNotFound&) {
            return true;
        }
    };
    erase(std::remove_if(begin(), end(), not_in_country), end());
    return *this;

} // AntigenRefs::country

// ----------------------------------------------------------------------

AntigenRefs& AntigenRefs::date_range(std::string aBegin, std::string aEnd)
{
    auto before_begin = [&aBegin](const auto& e) -> bool {
        const auto date = e->date();
        return date.empty() || date < aBegin;
    };
    auto after_end = [&aEnd](const auto& e) -> bool {
        const auto date = e->date();
        return date.empty() || date >= aEnd;
    };
    auto not_between = [&aBegin,&aEnd](const auto& e) -> bool {
        const auto date = e->date();
        return date.empty() || date < aBegin || date >= aEnd;
    };

    if (aBegin.empty()) {
        if (!aEnd.empty())
            erase(std::remove_if(begin(), end(), after_end), end());
    }
    else {
        if (aEnd.empty())
            erase(std::remove_if(begin(), end(), before_begin), end());
        else
            erase(std::remove_if(begin(), end(), not_between), end());
    }
    return *this;

} // AntigenRefs::date_range

// ----------------------------------------------------------------------

void Antigens::make_index(const HiDb& aHiDb)
{
    std::smatch m;
    for (const auto& antigen: *this) {
        try {
            const std::string key = index_key(antigen.data().name());
            auto p = mIndex.find(key);
            if (p == mIndex.end()) {
                p = mIndex.emplace(key, aHiDb).first;
            }
            p->second.push_back(&antigen);
        }
        catch (NotFound&) {
        }
    }
      // std::cerr << size() << " antigens " << mIndex.size() << " index entries" << std::endl;

} // Antigens::make_index

// ----------------------------------------------------------------------

AntigenRefs Antigens::find_by_index(std::string name, const HiDb& aHiDb) const
{
    AntigenRefs result;
    try {
        std::string n_virus_type, n_host, n_location, n_isolation, n_year, n_passage, n_key;
        split(name, n_virus_type, n_host, n_location, n_isolation, n_year, n_passage, n_key);
        try {
            const auto location = aHiDb.locdb().find(n_location);
            n_key = location.name.substr(0, IndexKeySize);
            const AntigenRefs* fk = for_key(n_key);
            if (fk) {
                result = *fk;
                auto not_match_fields = [&](const auto& e) -> bool {
                    std::string f_virus_type, f_host, f_location, f_isolation, f_year, f_passage, f_key;
                    this->split(e->data().name(), f_virus_type, f_host, f_location, f_isolation, f_year, f_passage, f_key); // gcc 6.2 wants this->
                    return f_host != n_host || f_location != location.name || f_isolation != n_isolation || f_year != n_year;
                };
                result.erase(std::remove_if(result.begin(), result.end(), not_match_fields), result.end());
            }
        }
        catch (LocationNotFound&) {
              // location not found in locdb, makes no sense looking up
            std::cerr << "LocationNotFound " << n_location << std::endl;
        }
    }
    catch (NotFound&) {
        if (name.size() > 3 && name[2] == ' ') { // CDC name?
            find_by_index_cdc_name(name, result);
        }
    }
    return result;

} // Antigens::find_by_index

// ----------------------------------------------------------------------

void Antigens::find_by_index_cdc_name(std::string name, AntigenRefs& aResult) const
{
    const std::string key = index_key(name);
    const AntigenRefs* fk = for_key(key);
    if (fk) {
        const auto found1 = std::find_if(fk->begin(), fk->end(), [&name](const auto& e) -> bool { return e->data().full_name() == name; });
        if (found1 != fk->end()) {
              // exact match
            aResult.push_back(*found1);
        }
        else {
              // try with prefix
            std::string prefix(name, 0, name.find(' ', 3));
            for (const auto& e: *fk) {
                if (e->data().full_name().substr(0, prefix.size()) == prefix)
                    aResult.push_back(e);
            }

            if (aResult.empty() && name[2] == ' ') {
                  // use all names with matching cdc abbreviation as a suggestion
                std::copy_if(fk->begin(), fk->end(), std::back_inserter(aResult), [&name](const auto& e) -> bool { return e->data().full_name().substr(0, 2) == name.substr(0, 2); });
            }
        }
    }
    else {
          //std::cerr << "find_by_index not recognized name: " << name << " index-key: " << key << " " << (fk ? fk->size() : 0) << std::endl;
          // Not a recognized name, ignore it
    }

} // Antigens::find_by_index_cdc_name

// ----------------------------------------------------------------------

AntigenRefs Antigens::find_by_cdcid(std::string cdcid) const
{
    if (std::isdigit(cdcid[0]))
        cdcid = "CDC#" + cdcid;
    AntigenRefs result;
    for (const auto& antigen: *this) {
        if (antigen.has_lab_id(cdcid))
            result.push_back(&antigen);
    }
    return result;

} // Antigens::find_by_cdcid

// ----------------------------------------------------------------------

void HiDb::add(const Chart& aChart)
{
    ChartData chart(aChart);
    std::cout << chart.table_id() << std::endl;
    mCharts.insert(mCharts.insert_pos(chart), std::move(chart));

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
    hidb_export(aFilename, *this, aPretty ? 1 : 0);

} // HiDb::exportTo

// ----------------------------------------------------------------------

void HiDb::importFrom(std::string aFilename)
{
    hidb_import(aFilename, *this);
    mAntigens.make_index(*this);

} // HiDb::importFrom

// ----------------------------------------------------------------------

template <typename Data> class FindScore
{
 private:
    static constexpr const string_match::score_t keyword_in_lookup = 1;

 public:
    inline FindScore(std::string name, const Data& aAntigen, string_match::score_t aNameScoreThreshold)
        : mAntigen(&aAntigen), mName(0), mFull(0)
        {
            preprocess(name, aNameScoreThreshold);
        }

    inline FindScore(std::string name, const Data* aAntigen, string_match::score_t aNameScoreThreshold)
        : mAntigen(aAntigen), mName(0), mFull(0)
        {
            preprocess(name, aNameScoreThreshold);
        }

    inline bool operator < (const FindScore<Data>& aNother) const
        {
              // if mFull == keyword_in_lookup, move it to the end of the sorting list regardless of mName
            bool result;
            if (mFull == keyword_in_lookup)
                result = false;
            else if (aNother.mFull == keyword_in_lookup)
                result = true;
            else
                result = mName == aNother.mName ? mFull > aNother.mFull : mName > aNother.mName;
            return result;
        }

    inline bool operator == (const FindScore<Data>& aNother) const { return mName == aNother.mName; }
    inline operator const Data*() const { return mAntigen; }
    inline operator const Data&() const { return *mAntigen; }
      // inline const Data* data() const { return mAntigen; }
    inline operator bool() const { return mName > 0; }
    inline string_match::score_t name_score() const { return mName; }
    inline std::pair<const Data*, size_t> score() const { return std::make_pair(mAntigen, mFull); }
    inline std::string full_name() const { return mAntigen->data().full_name(); }

 private:
    const Data* mAntigen;
    string_match::score_t mName, mFull;

    inline void preprocess(std::string name, string_match::score_t aNameScoreThreshold)
        {
            const auto antigen_name = mAntigen->data().name();
            mName = string_match::match(antigen_name, name);
            if (aNameScoreThreshold == 0)
                aNameScoreThreshold = static_cast<string_match::score_t>(name.length() * name.length() * 0.05);
            if (mName >= aNameScoreThreshold) {
                const auto full_name = mAntigen->data().full_name();
                mFull = std::max({
                    for_subst(full_name, antigen_name.size(), name, " CELL", {" MDCK", " SIAT"}, {}),
                    for_subst(full_name, antigen_name.size(), name, " EGG", {" E"}, {"NYMC", "IVR", "NIB", "RESVIR", "RG", "VI", "REASSORTANT"}),
                    for_subst(full_name, antigen_name.size(), name, " REASSORTANT", {" NYMC", " IVR", " NIB", " RESVIR", " RG", " VI", " REASSORTANT"}, {})
                    });
                if (mFull == 0)
                    mFull = string_match::match(full_name, name);
                  // std::cerr << mName << " " << mFull << " " << full_name << std::endl;
            }
        }

    inline string_match::score_t for_subst(std::string full_name, size_t name_part_size, std::string name, std::string keyword, std::initializer_list<const char*>&& subst_list, std::initializer_list<const char*>&& negative_list)
    {
        string_match::score_t score = 0;
        const auto pos = name.find(keyword);
        if (pos != std::string::npos) { // keyword is in the lookup name
            if (!std::any_of(negative_list.begin(), negative_list.end(), [&full_name,name_part_size](const auto& e) -> bool { return full_name.find(e, name_part_size) != std::string::npos; })) {
                for (const auto& subst: subst_list) {
                    if (full_name.find(subst + 1, name_part_size) != std::string::npos) { // subst (without leading space) must be present in full_name in the passage part
                        std::string substituted(name, 0, pos);
                        substituted.append(subst);
                        score = std::max(score, string_match::match(full_name, substituted));
                    }
                    else if (score == 0)
                        score = keyword_in_lookup; // to avoid using name-with-keyword-not-replaced for matching
                }
            }
            else                // string from negative_list present in full_name, ignore this name
                score = keyword_in_lookup;
        }
        return score;
    }
};

typedef FindScore<AntigenData> FindAntigenScore;
typedef FindScore<SerumData> FindSerumScore;

// ----------------------------------------------------------------------

template <typename AntigenT, typename Data> inline static void find_scores(std::string name, const std::vector<AntigenT>& antigens, std::vector<FindScore<Data>>& scores, typename std::vector<FindScore<Data>>::iterator& scores_end)
{
    string_match::score_t score_threshold = 0;
    for (const AntigenT& antigen: antigens) {
        scores.emplace_back(name, antigen, score_threshold);
        score_threshold = std::max(scores.back().name_score(), score_threshold);
    }
    std::sort(scores.begin(), scores.end());
    scores_end = std::find_if_not(scores.begin(), scores.end(), [&scores](const auto& e) { return e == scores.front(); }); // just use entries with maximal name score

} // HiDb::find_scores

// ----------------------------------------------------------------------

template <typename AntigenT> inline static FindScore<AntigenT> find_best_score(std::string name, const std::vector<AntigenT*>& antigens)
{
    string_match::score_t score_threshold = 0;
    FindScore<AntigenT> result{name, antigens.front(), score_threshold};
    for (const AntigenT* antigen: antigens) {
        const FindScore<AntigenT> score{name, *antigen, score_threshold};
        if (score < result)
            result = score;
        score_threshold = std::max(score.name_score(), score_threshold);
    }
    return result;

} // HiDb::find_scores

// ----------------------------------------------------------------------

std::vector<const AntigenData*> HiDb::find_antigens(std::string name_reassortant_annotations_passage) const
{
    AntigenRefs by_name = mAntigens.find_by_index(name_reassortant_annotations_passage, *this);
    std::vector<FindAntigenScore> scores;
    std::vector<FindAntigenScore>::iterator scores_end;
    find_scores(name_reassortant_annotations_passage, by_name, scores, scores_end);
    return {scores.begin(), scores_end};

} // HiDb::find_antigens

// ----------------------------------------------------------------------

const AntigenData& HiDb::find_antigen_exactly(std::string name_reassortant_annotations_passage) const
{
    AntigenRefs by_name = mAntigens.find_by_index(name_reassortant_annotations_passage, *this);
    auto found = std::find_if(by_name.begin(), by_name.end(), [&](const auto* a) -> bool { return name_reassortant_annotations_passage == a->data().name_for_exact_matching(); });
    if (found == by_name.end())
        throw NotFound(name_reassortant_annotations_passage, by_name);
    return **found;

} // HiDb::find_antigen_exactly

// ----------------------------------------------------------------------

std::vector<const AntigenData*> HiDb::find_antigens_fuzzy(std::string name_reassortant_annotations_passage) const
{
    const AntigenRefs* by_index = mAntigens.all_by_index(name_reassortant_annotations_passage);
    if (by_index) {
        std::vector<FindAntigenScore> scores;
        std::vector<FindAntigenScore>::iterator scores_end;
        find_scores(name_reassortant_annotations_passage, *by_index, scores, scores_end);
        return {scores.begin(), scores_end};
    }
    else {
        return {};
    }

} // HiDb::find_antigens_fuzzy

// ----------------------------------------------------------------------

std::vector<const AntigenData*> HiDb::find_antigens_extra_fuzzy(std::string name_reassortant_annotations_passage) const
{
    std::vector<FindAntigenScore> scores;
    std::vector<FindAntigenScore>::iterator scores_end;
    find_scores(name_reassortant_annotations_passage, antigens(), scores, scores_end);
    return {scores.begin(), scores_end};

} // HiDb::find_antigens_extra_fuzzy

// ----------------------------------------------------------------------

std::vector<std::pair<const AntigenData*, size_t>> HiDb::find_antigens_with_score(std::string name) const
{
    std::vector<FindAntigenScore> scores;
    std::vector<FindAntigenScore>::iterator scores_end;
    find_scores(name, antigens(), scores, scores_end);
    std::vector<std::pair<const AntigenData*, size_t>> result;
    std::transform(scores.begin(), scores_end, std::back_inserter(result), [](const auto& e) { return e.score(); });
    return result;

} // HiDb::find_antigens_with_score

// ----------------------------------------------------------------------

std::vector<std::string> HiDb::list_antigens() const
{
    std::vector<std::string> result;
    std::transform(antigens().begin(), antigens().end(), std::back_inserter(result), [](const auto& ag) -> std::string { return ag.data().name(); });
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;

} // HiDb::list_antigens

// ----------------------------------------------------------------------

std::vector<const SerumData*> HiDb::find_sera(std::string name) const
{
    std::vector<FindSerumScore> scores;
    std::vector<FindSerumScore>::iterator scores_end;
    find_scores(name, sera(), scores, scores_end);
    return {scores.begin(), scores_end};

} // HiDb::find_sera

// ----------------------------------------------------------------------

const SerumData& HiDb::find_serum_exactly(std::string name_reassortant_annotations_serum_id) const
{
    const auto found = std::find_if(mSera.begin(), mSera.end(), [&](const auto& sr) -> bool { return name_reassortant_annotations_serum_id == sr.data().name_for_exact_matching(); });
    if (found == mSera.end()) {
        // std::cerr << "find_serum_exactly \"" << name_reassortant_annotations_serum_id << '"' << std::endl;
        // for (const auto& sr: mSera) std::cerr << "  \"" << sr.data().name_for_exact_matching() << '"' << std::endl;
        throw NotFound(name_reassortant_annotations_serum_id);
    }
    return *found;

} // HiDb::find_serum_exactly

// ----------------------------------------------------------------------

std::vector<std::pair<const SerumData*, size_t>> HiDb::find_sera_with_score(std::string name) const
{
    std::vector<FindSerumScore> scores;
    std::vector<FindSerumScore>::iterator scores_end;
    find_scores(name, sera(), scores, scores_end);
    std::vector<std::pair<const SerumData*, size_t>> result;
    std::transform(scores.begin(), scores_end, std::back_inserter(result), [](const auto& e) { return e.score(); });
    return result;

} // HiDb::find_sera_with_score

// ----------------------------------------------------------------------

std::vector<std::string> HiDb::list_sera() const
{
    std::vector<std::string> result;
    std::transform(sera().begin(), sera().end(), std::back_inserter(result), [](const auto& sr) -> std::string { return sr.data().name(); });
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;

} // HiDb::list_sera

// ----------------------------------------------------------------------

std::vector<std::string> HiDb::all_countries() const
{
    std::vector<std::string> result;
    std::transform(antigens().begin(), antigens().end(), std::back_inserter(result), [](const auto& ag) -> std::string { return ag.data().location(); });
      // Note: cdc_abbreviation starts with #
    std::sort(result.begin(), result.end());
    auto last = std::unique(result.begin(), result.end());
    last = std::remove(result.begin(), last, std::string()); // remove empty name meaning no location in the name detected, i.e. unrecognized name
    std::transform(result.begin(), last, result.begin(), [this](const auto& name) -> std::string { try { return mLocDb.country(name); } catch (LocationNotFound&) { return "**UNKNOWN"; } });
    std::sort(result.begin(), last);
    last = std::unique(result.begin(), last);
    result.erase(last, result.end());
    return result;

} // HiDb::all_countries

// ----------------------------------------------------------------------

std::vector<std::string> HiDb::unrecognized_locations() const
{
    std::vector<std::string> result;
    std::transform(antigens().begin(), antigens().end(), std::back_inserter(result), [](const auto& ag) -> std::string { return ag.data().location(); });
    std::transform(sera().begin(), sera().end(), std::back_inserter(result), [](const auto& sr) -> std::string { return sr.data().location(); });
    std::sort(result.begin(), result.end());
    auto last = std::unique(result.begin(), result.end());
    last = std::remove(result.begin(), last, std::string()); // remove empty name meaning no location in the name detected, i.e. unrecognized name
    std::transform(result.begin(), last, result.begin(), [this](const auto& name) -> std::string { try { mLocDb.find(name); return std::string(); } catch (LocationNotFound&) { return name; } });
    std::sort(result.begin(), last);
    last = std::unique(result.begin(), last);
    last = std::remove(result.begin(), last, std::string()); // remove empty which mean recognized locations
    result.erase(last, result.end());
    return result;

} // HiDb::unrecognized_locations

// ----------------------------------------------------------------------

HiDbAntigenStat HiDb::stat() const
{
    size_t total = 0;

    HiDbAntigenStat stat;
    std::string previous_name;
    for (const auto& antigen: antigens()) {
        if (antigen.data().name() != previous_name) {
            const auto continent = locdb().continent(antigen.data().location(), "UNKNOWN");
            YearMonth date = antigen.date();
            if (date.empty()) {
                date = antigen.data().year();
                if (date.empty())
                    date = "????";
            }
            else {
                date = date.substr(0, 4) + date.substr(5, 2);
            }
            const auto& table = mCharts[antigen.per_table().front().table_id()].chart_info();
            ++stat[table.virus_type()][table.lab()][date][continent];
            ++total;
            previous_name = antigen.data().name();
        }
    }
    std::cerr << "Total: " << total << std::endl;
    return stat;

} // HiDb::stat

// ----------------------------------------------------------------------

const hidb::HiDb& HiDbSet::get(std::string aVirusType) const
{
    auto h = mPtrs.find(aVirusType);
    if (h == mPtrs.end()) {
        std::string filename;
        if (aVirusType == "A(H1N1)")
            filename = mHiDbDir + "/hidb4.h1.json.xz";
        else if (aVirusType == "A(H3N2)")
            filename = mHiDbDir + "/hidb4.h3.json.xz";
        else if (aVirusType == "B")
            filename = mHiDbDir + "/hidb4.b.json.xz";
        else
            throw NoHiDb{};
          //throw std::runtime_error("No HiDb for " + aVirusType);

        std::unique_ptr<HiDb> hidb{new HiDb{}};
          // std::cout << "opening " << filename << std::endl;
        hidb->importFrom(filename);
        hidb->importLocDb(mHiDbDir + "/locationdb.json.xz");
        h = mPtrs.emplace(aVirusType, std::move(hidb)).first;
    }
    return *h->second;

} // HiDbSet::get

// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
/// Local Variables:
/// eval: (if (fboundp 'eu-rename-buffer) (eu-rename-buffer))
/// End:
