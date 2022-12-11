#include(GenerateExportHeader)
#generate_export_header(meshlink BASE_NAME meshlink)

add_library(meshlink SHARED
	adns.c
	buffer.c
	chacha-poly1305/chacha-poly1305.c
	chacha-poly1305/chacha.c
	chacha-poly1305/poly1305.c
	conf.c
	connection.c
	crypto.c
	devtools.c
	discovery.c
	dropin.c
	ed25519/add_scalar.c
	ed25519/ecdh.c
	ed25519/ecdsa.c
	ed25519/ecdsagen.c
	ed25519/fe.c
	ed25519/ge.c
	ed25519/key_exchange.c
	ed25519/keypair.c
	ed25519/sc.c
	ed25519/seed.c
	ed25519/sha512.c
	ed25519/sign.c
	ed25519/verify.c
	edge.c
	event.c
	graph.c
	hash.c
	list.c
	logger.c
	mdns.c
	meshlink.c
	meta.c
	net.c
	net_packet.c
	net_setup.c
	net_socket.c
	netutl.c
	node.c
	prf.c
	protocol.c
	protocol_auth.c
	protocol_edge.c
	protocol_key.c
	protocol_misc.c
	route.c
	splay_tree.c
	sptps.c
	submesh.c
	utcp-test.c
	utcp.c
	utils.c
	xoshiro.c
)

set(MESHLINK_PUBLIC_HEADERS
	../include/meshlink.h
	../include/meshlink++.h
)

target_compile_features(meshlink PUBLIC c_std_11)
target_include_directories(meshlink PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
	$<INSTALL_INTERFACE:include/meshlink>
)
target_link_libraries(meshlink PRIVATE Threads::Threads)
set_target_properties(meshlink PROPERTIES PUBLIC_HEADER "${MESHLINK_PUBLIC_HEADERS}")
set_property(TARGET meshlink PROPERTY C_VISIBILITY_PRESET hidden)
set_property(TARGET meshlink PROPERTY C_STANDARD 11)

include(GNUInstallDirs)
install(TARGETS meshlink
	LIBRARY
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/meshlink
)
