#include "nixsection.h"
#include "mex.h"
#include <nix.hpp>

#include "handle.h"
#include "arguments.h"
#include "struct.h"
#include "mknix.h"


namespace nixsection {

mxArray *describe(const nix::Section &section)
{
    struct_builder sb({ 1 }, { 
        "name", "id", "type", "definition", "repository", "mapping"
    });

    sb.set(section.name());
    sb.set(section.id());
    sb.set(section.type());
    sb.set(section.definition());
    sb.set(section.repository());
    sb.set(section.mapping());

    return sb.array();
}

void properties(const extractor &input, infusor &output)
{
    nix::Section section = input.entity<nix::Section>(1);
    std::vector<nix::Property> properties = section.properties();

    const mwSize size = static_cast<mwSize>(properties.size());
    mxArray *lst = mxCreateCellArray(1, &size);

    for (size_t i = 0; i < properties.size(); i++) {

        nix::Property pr = properties[i];
        std::vector<nix::Value> values = pr.values();

        mxArray *mx_values = make_mx_array(values);

        struct_builder sb({ 1 }, {
            "name", "id", "definition", "mapping", "unit", "values"
        });

        sb.set(pr.name());
        sb.set(pr.id());
        sb.set(pr.definition());
        sb.set(pr.mapping());
        sb.set(pr.unit());
        sb.set(mx_values);

        mxSetCell(lst, i, sb.array());
    }

    output.set(0, lst);
}

void create_property(const extractor &input, infusor &output)
{
    nix::Section currObj = input.entity<nix::Section>(1);

    nix::DataType dtype = nix::string_to_data_type(input.str(3));

    nix::Property p = currObj.createProperty(input.str(2), dtype);
    output.set(0, handle(p));
}

void create_property_with_value(const extractor &input, infusor &output)
{
    nix::Section currObj = input.entity<nix::Section>(1);

    std::vector<nix::Value> vals = input.vec(3);
    nix::Property p = currObj.createProperty(input.str(2), vals);
    output.set(0, handle(p));
}

} // namespace nixsection
