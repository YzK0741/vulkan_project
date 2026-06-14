& glslc ./base.frag -o ./base_frag.spv
& glslc ./base.vert -o ./base_vert.spv
& glslc ./pbr.vert -o ./pbr_vert.spv
& glslc ./pbr.frag -o ./pbr_frag.spv
& xxd -i base_vert.spv base_vertex_byte.bin
& xxd -i base_frag.spv base_fragment_byte.bin
& xxd -i pbr_vert.spv pbr_vertex_byte.bin
& xxd -i pbr_frag.spv pbr_fragment_byte.bin