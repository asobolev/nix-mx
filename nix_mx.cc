#include <iostream>
#include <math.h>
#include <string.h>
#include <vector>

#include "handle.h" // will include nix.h, mex.h
#include "arguments.h"
#include "struct.h"
#include "mknix.h"

#include "nixfile.h"
#include "nixsection.h"
#include "nixproperty.h"
#include "nixblock.h"
#include "nixdataarray.h"
#include "nixsource.h"
#include "nixfeature.h"
#include "nixtag.h"
#include "nixmultitag.h"
#include "nixdimensions.h"

#include <utils/glue.h>

#include <mutex>

// *** functions ***

static void entity_destroy(const extractor &input, infusor &output)
{
    handle h = input.hdl(1);
    h.destroy();
}

static void entity_updated_at(const extractor &input, infusor &output)
{
    const handle::entity *curr = input.hdl(1).the_entity();
    time_t uat = curr->updated_at();
    uint64_t the_time = static_cast<uint64_t>(uat);
    output.set(0, the_time);
}

// *** ***

//glue "globals"
std::once_flag init_flag;
static glue::registry *methods = nullptr;

static void on_exit() {
#ifdef DEBUG_GLUE
    mexPrintf("[GLUE] deleting handlers!\n");
#endif

    delete methods;
}

#define GETTER(type, class, name) static_cast<type(class::*)()const>(&class::name)
#define FILTER(type, class, filt, name) static_cast<type(class::*)(filt)const>(&class::name)
#define SETTER(type, class, name) static_cast<void(class::*)(type)>(&class::name)
#define REMOVER(type, class, name) static_cast<bool(class::*)(const std::string&)>(&class::name)
#define GETBYSTR(type, class, name) static_cast<type(class::*)(const std::string &)const>(&class::name)
#define GETCONTENT(type, class, name) static_cast<type(class::*)()const>(&class::name)

//required to operate on DataArray, Visual Studio 12 compiler does not resolve multiple inheritance properly
#define IDATAARRAY(type, iface, attr, name, isconst) static_cast<type(nix::base::iface<nix::base::IDataArray>::*)(attr)isconst>(&nix::base::iface<nix::base::IDataArray>::name)


// main entry point
void mexFunction(int            nlhs,
    mxArray       *lhs[],
    int            nrhs,
    const mxArray *rhs[])
{
    extractor input(rhs, nrhs);
    infusor   output(lhs, nlhs);

    std::string cmd = input.str(0);

    //mexPrintf("[F] %s\n", cmd.c_str());

    std::call_once(init_flag, []() {
        using namespace glue;

#ifdef DEBUG_GLUE
        mexPrintf("[GLUE] Registering classdefs...\n");
#endif

        methods = new registry{};

        methods->add("Entity::destroy", entity_destroy);
        methods->add("Entity::updatedAt", entity_updated_at);

        classdef<nix::File>("File", methods)
            .desc(&nixfile::describe)
            .add("open", nixfile::open)
            .reg("blocks", GETTER(std::vector<nix::Block>, nix::File, blocks))
            .reg("sections", GETTER(std::vector<nix::Section>, nix::File, sections))
            .reg("deleteBlock", REMOVER(nix::Block, nix::File, deleteBlock))
            .reg("deleteSection", REMOVER(nix::Section, nix::File, deleteSection))
            .reg("openBlock", GETBYSTR(nix::Block, nix::File, getBlock))
            .reg("openSection", GETBYSTR(nix::Section, nix::File, getSection))
            .reg("createBlock", &nix::File::createBlock)
            .reg("createSection", &nix::File::createSection);

        classdef<nix::Block>("Block", methods)
            .desc(&nixblock::describe)
            .reg("createSource", &nix::Block::createSource)
            .reg("createTag", &nix::Block::createTag)
            .reg("dataArrays", &nix::Block::dataArrays)
            .reg("sources", &nix::Block::sources)
            .reg("tags", &nix::Block::tags)
            .reg("multiTags", &nix::Block::multiTags)
            .reg("hasTag", GETBYSTR(bool, nix::Block, hasTag))
            .reg("hasMultiTag", GETBYSTR(bool, nix::Block, hasMultiTag))
            .reg("openDataArray", GETBYSTR(nix::DataArray, nix::Block, getDataArray))
            .reg("openSource", GETBYSTR(nix::Source, nix::Block, getSource))
            .reg("openTag", GETBYSTR(nix::Tag, nix::Block, getTag))
            .reg("openMultiTag", GETBYSTR(nix::MultiTag, nix::Block, getMultiTag))
            .reg("openMetadataSection", GETCONTENT(nix::Section, nix::Block, metadata))
            .reg("set_metadata", SETTER(const std::string&, nix::Block, metadata))
            .reg("set_none_metadata", SETTER(const boost::none_t, nix::Block, metadata))
            .reg("deleteDataArray", REMOVER(nix::DataArray, nix::Block, deleteDataArray))
            .reg("deleteSource", REMOVER(nix::Source, nix::Block, deleteSource))
            .reg("deleteTag", REMOVER(nix::Tag, nix::Block, deleteTag))
            .reg("deleteMultiTag", REMOVER(nix::MultiTag, nix::Block, deleteMultiTag))
            .reg("set_type", SETTER(const std::string&, nix::Block, type))
            .reg("set_definition", SETTER(const std::string&, nix::Block, definition))
            .reg("set_none_definition", SETTER(const boost::none_t, nix::Block, definition));
        methods->add("Block::createDataArray", nixblock::create_data_array);
        methods->add("Block::createMultiTag", nixblock::create_multi_tag);

        classdef<nix::DataArray>("DataArray", methods)
            .desc(&nixdataarray::describe)
            .reg("sources", IDATAARRAY(std::vector<nix::Source>, EntityWithSources, std::function<bool(const nix::Source &)>, sources, const))
            .reg("openMetadataSection", IDATAARRAY(nix::Section, EntityWithMetadata, , metadata, const))
            .reg("set_metadata", IDATAARRAY(void, EntityWithMetadata, const std::string&, metadata, ))
            .reg("set_none_metadata", IDATAARRAY(void, EntityWithMetadata, const boost::none_t, metadata, ))
            .reg("set_type", IDATAARRAY(void, NamedEntity, const std::string&, type, ))
            .reg("set_definition", IDATAARRAY(void, NamedEntity, const std::string&, definition, ))
            .reg("set_none_definition", IDATAARRAY(void, NamedEntity, const boost::none_t, definition, ))
            .reg("set_label", SETTER(const std::string&, nix::DataArray, label))
            .reg("set_none_label", SETTER(const boost::none_t, nix::DataArray, label))
            .reg("set_unit", SETTER(const std::string&, nix::DataArray, unit))
            .reg("set_none_unit", SETTER(const boost::none_t, nix::DataArray, unit))
            .reg("dimensions", FILTER(std::vector<nix::Dimension>, nix::DataArray, , dimensions))
            .reg("append_set_dimension", &nix::DataArray::appendSetDimension)
            .reg("append_range_dimension", &nix::DataArray::appendRangeDimension)
            .reg("append_sampled_dimension", &nix::DataArray::appendSampledDimension)
            .reg("create_set_dimension", &nix::DataArray::createSetDimension)
            .reg("create_range_dimension", &nix::DataArray::createRangeDimension)
            .reg("create_sampled_dimension", &nix::DataArray::createSampledDimension);
        methods->add("DataArray::delete_dimension", nixdataarray::delete_dimension);
        methods->add("DataArray::readAll", nixdataarray::read_all);
        methods->add("DataArray::writeAll", nixdataarray::write_all);
        methods->add("DataArray::addSource", nixdataarray::add_source);
        // REMOVER for DataArray.removeSource leads to an error, therefore use method->add for now
        methods->add("DataArray::removeSource", nixdataarray::remove_source);

        classdef<nix::Source>("Source", methods)
            .desc(&nixsource::describe)
            .reg("createSource", &nix::Source::createSource)
            .reg("deleteSource", REMOVER(nix::Source, nix::Source, deleteSource))
            .reg("sources", &nix::Source::sources)
            .reg("openSource", GETBYSTR(nix::Source, nix::Source, getSource))
            .reg("openMetadataSection", GETCONTENT(nix::Section, nix::Source, metadata))
            .reg("set_metadata", SETTER(const std::string&, nix::Source, metadata))
            .reg("set_none_metadata", SETTER(const boost::none_t, nix::Source, metadata))
            .reg("set_type", SETTER(const std::string&, nix::Source, type))
            .reg("set_definition", SETTER(const std::string&, nix::Source, definition))
            .reg("set_none_definition", SETTER(const boost::none_t, nix::Source, definition));

        classdef<nix::Tag>("Tag", methods)
            .desc(&nixtag::describe)
            .reg("references", GETTER(std::vector<nix::DataArray>, nix::Tag, references))
            .reg("features", &nix::Tag::features)
            .reg("sources", FILTER(std::vector<nix::Source>, nix::Tag, std::function<bool(const nix::Source &)>, sources))
            .reg("openReferenceDataArray", GETBYSTR(nix::DataArray, nix::Tag, getReference))
            .reg("openFeature", GETBYSTR(nix::Feature, nix::Tag, getFeature))
            .reg("openSource", GETBYSTR(nix::Source, nix::Tag, getSource))
            .reg("openMetadataSection", GETCONTENT(nix::Section, nix::Tag, metadata))
            .reg("set_units", SETTER(const std::vector<std::string>&, nix::Tag, units))
            .reg("set_none_units", SETTER(const boost::none_t, nix::Tag, units))
            .reg("set_metadata", SETTER(const std::string&, nix::Tag, metadata))
            .reg("set_none_metadata", SETTER(const boost::none_t, nix::Tag, metadata))
            .reg("set_type", SETTER(const std::string&, nix::Tag, type))
            .reg("set_definition", SETTER(const std::string&, nix::Tag, definition))
            .reg("set_none_definition", SETTER(const boost::none_t, nix::Tag, definition))
            .reg("removeReference", REMOVER(nix::DataArray, nix::Tag, removeReference))
            .reg("removeSource", REMOVER(nix::Source, nix::Tag, removeSource))
            .reg("deleteFeature", REMOVER(nix::Feature, nix::Tag, deleteFeature));
        methods->add("Tag::retrieveData", nixtag::retrieve_data);
        methods->add("Tag::featureRetrieveData", nixtag::retrieve_feature_data);
        methods->add("Tag::addReference", nixtag::add_reference);
        methods->add("Tag::addSource", nixtag::add_source);
        methods->add("Tag::createFeature", nixtag::create_feature);

        classdef<nix::MultiTag>("MultiTag", methods)
            .desc(&nixmultitag::describe)
            .reg("references", GETTER(std::vector<nix::DataArray>, nix::MultiTag, references))
            .reg("features", &nix::MultiTag::features)
            .reg("sources", FILTER(std::vector<nix::Source>, nix::MultiTag, std::function<bool(const nix::Source &)>, sources))
            .reg("hasPositions", GETCONTENT(bool, nix::MultiTag, hasPositions))
            .reg("openPositions", GETCONTENT(nix::DataArray, nix::MultiTag, positions))
            .reg("openExtents", GETCONTENT(nix::DataArray, nix::MultiTag, extents))
            .reg("openReferences", GETBYSTR(nix::DataArray, nix::MultiTag, getReference))
            .reg("openFeature", GETBYSTR(nix::Feature, nix::MultiTag, getFeature))
            .reg("openSource", GETBYSTR(nix::Source, nix::MultiTag, getSource))
            .reg("openMetadataSection", GETCONTENT(nix::Section, nix::MultiTag, metadata))
            .reg("set_metadata", SETTER(const std::string&, nix::MultiTag, metadata))
            .reg("set_none_metadata", SETTER(const boost::none_t, nix::MultiTag, metadata))
            .reg("removeReference", REMOVER(nix::DataArray, nix::MultiTag, removeReference))
            .reg("removeSource", REMOVER(nix::Source, nix::MultiTag, removeSource))
            .reg("deleteFeature", REMOVER(nix::Feature, nix::MultiTag, deleteFeature));
        methods->add("MultiTag::retrieveData", nixmultitag::retrieve_data);
        methods->add("MultiTag::featureRetrieveData", nixmultitag::retrieve_feature_data);
        methods->add("MultiTag::addReference", nixmultitag::add_reference);
        methods->add("MultiTag::addSource", nixmultitag::add_source);
        methods->add("MultiTag::createFeature", nixmultitag::create_feature);
        methods->add("MultiTag::addPositions", nixmultitag::add_positions);
        methods->add("MultiTag::addExtents", nixmultitag::add_extents);

        classdef<nix::Section>("Section", methods)
            .desc(&nixsection::describe)
            .reg("sections", &nix::Section::sections)
            .reg("openSection", GETBYSTR(nix::Section, nix::Section, getSection))
            .reg("hasProperty", GETBYSTR(bool, nix::Section, hasProperty))
            .reg("hasSection", GETBYSTR(bool, nix::Section, hasSection))
            .reg("openLink", GETCONTENT(nix::Section, nix::Section, link))
            .reg("set_link", SETTER(const std::string&, nix::Section, link))
            .reg("set_none_link", SETTER(const boost::none_t, nix::Section, link))
            .reg("parent", GETCONTENT(nix::Section, nix::Section, parent))
            .reg("set_type", SETTER(const std::string&, nix::Section, type))
            .reg("set_definition", SETTER(const std::string&, nix::Section, definition))
            .reg("set_none_definition", SETTER(const boost::none_t, nix::Section, definition))
            .reg("set_repository", SETTER(const std::string&, nix::Section, repository))
            .reg("set_none_repository", SETTER(const boost::none_t, nix::Section, repository))
            .reg("set_mapping", SETTER(const std::string&, nix::Section, mapping))
            .reg("set_none_mapping", SETTER(const boost::none_t, nix::Section, mapping))
            .reg("createSection", &nix::Section::createSection)
            .reg("deleteSection", REMOVER(nix::Section, nix::Section, deleteSection))
            .reg("openProperty", GETBYSTR(nix::Property, nix::Section, getProperty))
            .reg("deleteProperty", REMOVER(nix::Property, nix::Section, deleteProperty));
        methods->add("Section::properties", nixsection::properties);
        methods->add("Section::createProperty", nixsection::create_property);
        methods->add("Section::createPropertyWithValue", nixsection::create_property_with_value);

        classdef<nix::Feature>("Feature", methods)
            .desc(&nixfeature::describe)
            .reg("openData", GETCONTENT(nix::DataArray, nix::Feature, data));

        classdef<nix::Property>("Property", methods)
            .desc(&nixproperty::describe)
            .reg("set_definition", SETTER(const std::string&, nix::Property, definition))
            .reg("set_none_definition", SETTER(const boost::none_t, nix::Property, definition))
            .reg("set_unit", SETTER(const std::string&, nix::Property, unit))
            .reg("set_none_unit", SETTER(const boost::none_t, nix::Property, unit))
            .reg("set_mapping", SETTER(const std::string&, nix::Property, mapping))
            .reg("set_none_mapping", SETTER(const boost::none_t, nix::Property, mapping));
        methods->add("Property::values", nixproperty::values);
        methods->add("Property::updateValues", nixproperty::update_values);

        classdef<nix::SetDimension>("SetDimension", methods)
            .desc(&nixdimensions::describe)
            .reg("set_labels", SETTER(const std::vector<std::string>&, nix::SetDimension, labels))
            .reg("set_none_labels", SETTER(const boost::none_t, nix::SetDimension, labels));

        classdef<nix::SampledDimension>("SampledDimension", methods)
            .desc(&nixdimensions::describe)
            .reg("set_label", SETTER(const std::string&, nix::SampledDimension, label))
            .reg("set_none_label", SETTER(const boost::none_t, nix::SampledDimension, label))
            .reg("set_unit", SETTER(const std::string&, nix::SampledDimension, unit))
            .reg("set_none_unit", SETTER(const boost::none_t, nix::SampledDimension, unit))
            .reg("set_samplingInterval", SETTER(double, nix::SampledDimension, samplingInterval))
            .reg("set_offset", SETTER(double, nix::SampledDimension, offset))
            .reg("set_none_offset", SETTER(const boost::none_t, nix::SampledDimension, offset))
            .reg("index_of", &nix::SampledDimension::indexOf)
            .reg("position_at", &nix::SampledDimension::positionAt)
            .reg("axis", &nix::SampledDimension::axis);
        
        classdef<nix::RangeDimension>("RangeDimension", methods)
            .desc(&nixdimensions::describe)
            .reg("set_label", SETTER(const std::string&, nix::RangeDimension, label))
            .reg("set_none_label", SETTER(const boost::none_t, nix::RangeDimension, label))
            .reg("set_unit", SETTER(const std::string&, nix::RangeDimension, unit))
            .reg("set_none_unit", SETTER(const boost::none_t, nix::RangeDimension, unit))
            .reg("set_ticks", SETTER(const std::vector<double>&, nix::RangeDimension, ticks))
            .reg("index_of", &nix::RangeDimension::indexOf)
            .reg("tick_at", &nix::RangeDimension::tickAt)
            .reg("axis", &nix::RangeDimension::axis);

        mexAtExit(on_exit);
    });

    bool processed = false;

    try {
        processed = methods->dispatch(cmd, input, output);

#ifdef DEBUG_GLUE
        if (processed) {
            mexPrintf("[GLUE] %s: processed by glue.\n", cmd.c_str());
        }
#endif

    }
    catch (const std::invalid_argument &e) {
        mexErrMsgIdAndTxt("nix:arg:inval", e.what());
    }
    catch (const std::exception &e) {
        mexErrMsgIdAndTxt("nix:arg:dispatch", e.what());
    }
    catch (...) {
        mexErrMsgIdAndTxt("nix:arg:dispatch", "unkown exception");
    }

    if (!processed) {
        mexErrMsgIdAndTxt("nix:arg:dispatch", "Unkown command");
    }
}

