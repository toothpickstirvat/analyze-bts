#include <fc/crypto/dh.hpp>

namespace fc {

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

// ── OpenSSL 3.0+ implementation using EVP_PKEY API ──────────────────────────

#include <openssl/param_build.h>
#include <openssl/core_names.h>

// Build an EVP_PKEY holding only DH parameters (p, g).
static EVP_PKEY* build_dh_params( const std::vector<char>& p_data, uint8_t g_val )
{
    BIGNUM* bn_p = BN_bin2bn( (const unsigned char*)p_data.data(), p_data.size(), nullptr );
    BIGNUM* bn_g = BN_new();
    BN_set_word( bn_g, g_val );

    OSSL_PARAM_BLD* bld = OSSL_PARAM_BLD_new();
    OSSL_PARAM_BLD_push_BN( bld, OSSL_PKEY_PARAM_FFC_P, bn_p );
    OSSL_PARAM_BLD_push_BN( bld, OSSL_PKEY_PARAM_FFC_G, bn_g );
    OSSL_PARAM* ossl_params = OSSL_PARAM_BLD_to_param( bld );

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name( nullptr, "DH", nullptr );
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_fromdata_init( ctx );
    EVP_PKEY_fromdata( ctx, &pkey, EVP_PKEY_KEY_PARAMETERS, ossl_params );

    EVP_PKEY_CTX_free( ctx );
    OSSL_PARAM_free( ossl_params );
    OSSL_PARAM_BLD_free( bld );
    BN_free( bn_p );
    BN_free( bn_g );
    return pkey;
}

bool diffie_hellman::generate_params( int s, uint8_t g_val )
{
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_from_name( nullptr, "DH", nullptr );
    EVP_PKEY_paramgen_init( pctx );
    EVP_PKEY_CTX_set_dh_paramgen_prime_len( pctx, s );
    EVP_PKEY_CTX_set_dh_paramgen_generator( pctx, g_val );

    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_paramgen( pctx, &pkey );
    EVP_PKEY_CTX_free( pctx );

    BIGNUM* bn_p = nullptr;
    EVP_PKEY_get_bn_param( pkey, OSSL_PKEY_PARAM_FFC_P, &bn_p );
    p.resize( BN_num_bytes( bn_p ) );
    if( !p.empty() ) BN_bn2bin( bn_p, (unsigned char*)p.data() );
    BN_free( bn_p );

    this->g = g_val;

    EVP_PKEY_CTX* vctx = EVP_PKEY_CTX_new( pkey, nullptr );
    valid = ( EVP_PKEY_param_check( vctx ) == 1 );
    EVP_PKEY_CTX_free( vctx );
    EVP_PKEY_free( pkey );
    return valid;
}

bool diffie_hellman::validate()
{
    if( p.empty() ) return valid = false;
    EVP_PKEY* pkey = build_dh_params( p, g );
    if( !pkey ) return valid = false;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new( pkey, nullptr );
    valid = ( EVP_PKEY_param_check( ctx ) == 1 );
    EVP_PKEY_CTX_free( ctx );
    EVP_PKEY_free( pkey );
    return valid;
}

bool diffie_hellman::generate_pub_key()
{
    if( p.empty() ) return valid = false;

    EVP_PKEY* params = build_dh_params( p, g );
    if( !params ) return valid = false;

    EVP_PKEY_CTX* vctx = EVP_PKEY_CTX_new( params, nullptr );
    if( EVP_PKEY_param_check( vctx ) != 1 )
    {
        EVP_PKEY_CTX_free( vctx );
        EVP_PKEY_free( params );
        return valid = false;
    }
    EVP_PKEY_CTX_free( vctx );

    EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new( params, nullptr );
    EVP_PKEY_keygen_init( kctx );
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_keygen( kctx, &pkey );
    EVP_PKEY_CTX_free( kctx );
    EVP_PKEY_free( params );

    BIGNUM* bn_pub  = nullptr;
    BIGNUM* bn_priv = nullptr;
    EVP_PKEY_get_bn_param( pkey, OSSL_PKEY_PARAM_PUB_KEY,  &bn_pub );
    EVP_PKEY_get_bn_param( pkey, OSSL_PKEY_PARAM_PRIV_KEY, &bn_priv );

    pub_key.resize(  BN_num_bytes( bn_pub  ) );
    priv_key.resize( BN_num_bytes( bn_priv ) );
    if( !pub_key.empty()  ) BN_bn2bin( bn_pub,  (unsigned char*)pub_key.data()  );
    if( !priv_key.empty() ) BN_bn2bin( bn_priv, (unsigned char*)priv_key.data() );

    BN_free( bn_pub );
    BN_free( bn_priv );
    EVP_PKEY_free( pkey );
    return valid = true;
}

bool diffie_hellman::compute_shared_key( const char* buf, uint32_t s )
{
    // Build our full key pair (p, g, pub_key, priv_key)
    BIGNUM* bn_p     = BN_bin2bn( (const unsigned char*)p.data(),        p.size(),        nullptr );
    BIGNUM* bn_g     = BN_new(); BN_set_word( bn_g, g );
    BIGNUM* bn_pub   = BN_bin2bn( (const unsigned char*)pub_key.data(),  pub_key.size(),  nullptr );
    BIGNUM* bn_priv  = BN_bin2bn( (const unsigned char*)priv_key.data(), priv_key.size(), nullptr );

    OSSL_PARAM_BLD* bld = OSSL_PARAM_BLD_new();
    OSSL_PARAM_BLD_push_BN( bld, OSSL_PKEY_PARAM_FFC_P,   bn_p    );
    OSSL_PARAM_BLD_push_BN( bld, OSSL_PKEY_PARAM_FFC_G,   bn_g    );
    OSSL_PARAM_BLD_push_BN( bld, OSSL_PKEY_PARAM_PUB_KEY,  bn_pub  );
    OSSL_PARAM_BLD_push_BN( bld, OSSL_PKEY_PARAM_PRIV_KEY, bn_priv );
    OSSL_PARAM* my_params = OSSL_PARAM_BLD_to_param( bld );

    EVP_PKEY_CTX* fctx = EVP_PKEY_CTX_new_from_name( nullptr, "DH", nullptr );
    EVP_PKEY* my_key = nullptr;
    EVP_PKEY_fromdata_init( fctx );
    EVP_PKEY_fromdata( fctx, &my_key, EVP_PKEY_KEYPAIR, my_params );
    EVP_PKEY_CTX_free( fctx );
    OSSL_PARAM_free( my_params );
    OSSL_PARAM_BLD_free( bld );

    // Build peer's public key (p, g, peer_pub)
    BIGNUM* bn_peer_pub = BN_bin2bn( (const unsigned char*)buf, s, nullptr );

    OSSL_PARAM_BLD* peer_bld = OSSL_PARAM_BLD_new();
    OSSL_PARAM_BLD_push_BN( peer_bld, OSSL_PKEY_PARAM_FFC_P,  bn_p        );
    OSSL_PARAM_BLD_push_BN( peer_bld, OSSL_PKEY_PARAM_FFC_G,  bn_g        );
    OSSL_PARAM_BLD_push_BN( peer_bld, OSSL_PKEY_PARAM_PUB_KEY, bn_peer_pub );
    OSSL_PARAM* peer_params = OSSL_PARAM_BLD_to_param( peer_bld );

    EVP_PKEY_CTX* pfctx = EVP_PKEY_CTX_new_from_name( nullptr, "DH", nullptr );
    EVP_PKEY* peer_key = nullptr;
    EVP_PKEY_fromdata_init( pfctx );
    EVP_PKEY_fromdata( pfctx, &peer_key, EVP_PKEY_PUBLIC_KEY, peer_params );
    EVP_PKEY_CTX_free( pfctx );
    OSSL_PARAM_free( peer_params );
    OSSL_PARAM_BLD_free( peer_bld );

    BN_free( bn_p );
    BN_free( bn_g );
    BN_free( bn_pub );
    BN_free( bn_priv );
    BN_free( bn_peer_pub );

    if( !my_key || !peer_key )
    {
        EVP_PKEY_free( my_key );
        EVP_PKEY_free( peer_key );
        return false;
    }

    // Derive shared secret
    EVP_PKEY_CTX* dctx = EVP_PKEY_CTX_new( my_key, nullptr );
    EVP_PKEY_derive_init( dctx );
    EVP_PKEY_derive_set_peer( dctx, peer_key );

    size_t secret_len = 0;
    EVP_PKEY_derive( dctx, nullptr, &secret_len );
    shared_key.resize( secret_len );
    EVP_PKEY_derive( dctx, (unsigned char*)shared_key.data(), &secret_len );
    shared_key.resize( secret_len );

    EVP_PKEY_CTX_free( dctx );
    EVP_PKEY_free( my_key );
    EVP_PKEY_free( peer_key );
    return true;
}

#else

// ── OpenSSL 1.x implementation using legacy DH API ──────────────────────────

static bool validate( const ssl_dh& dh, bool& valid ) {
    int check;
    DH_check(dh,&check);
    return valid = !(check /*& DH_CHECK_P_NOT_SAFE_PRIME*/);
}

bool diffie_hellman::generate_params( int s, uint8_t g )
{
    ssl_dh dh(DH_new());
    DH_generate_parameters_ex(dh.obj, s, g, NULL);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    const BIGNUM* bn_p;
    DH_get0_pqg(dh.obj, &bn_p, NULL, NULL);
    p.resize( BN_num_bytes( bn_p ) );
    if( p.size() )
        BN_bn2bin( bn_p, (unsigned char*)&p.front() );
#else
    p.resize( BN_num_bytes( dh->p ) );
    if( p.size() )
        BN_bn2bin( dh->p, (unsigned char*)&p.front() );
#endif
    this->g = g;
    return fc::validate( dh, valid );
}

bool diffie_hellman::validate()
{
    if( !p.size() )
        return valid = false;
    ssl_dh dh(DH_new());
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    const auto bn_p = BN_bin2bn( (unsigned char*)&p.front(), p.size(), NULL );
    const auto bn_g = BN_bin2bn( (unsigned char*)&g, 1, NULL );
    DH_set0_pqg(dh.obj, bn_p, NULL, bn_g);
#else
    dh->p = BN_bin2bn( (unsigned char*)&p.front(), p.size(), NULL );
    dh->g = BN_bin2bn( (unsigned char*)&g, 1, NULL );
#endif
    return fc::validate( dh, valid );
}

bool diffie_hellman::generate_pub_key()
{
    if( !p.size() )
        return valid = false;
    ssl_dh dh(DH_new());
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    const auto bn_p = BN_bin2bn( (unsigned char*)&p.front(), p.size(), NULL );
    const auto bn_g = BN_bin2bn( (unsigned char*)&g, 1, NULL );
    DH_set0_pqg(dh.obj, bn_p, NULL, bn_g);
#else
    dh->p = BN_bin2bn( (unsigned char*)&p.front(), p.size(), NULL );
    dh->g = BN_bin2bn( (unsigned char*)&g, 1, NULL );
#endif
    if( !fc::validate( dh, valid ) )
        return false;
    DH_generate_key(dh);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    const BIGNUM* bn_pub_key;
    const BIGNUM* bn_priv_key;
    DH_get0_key(dh.obj, &bn_pub_key, &bn_priv_key);
    pub_key.resize(  BN_num_bytes( bn_pub_key  ) );
    priv_key.resize( BN_num_bytes( bn_priv_key ) );
    if( pub_key.size()  ) BN_bn2bin( bn_pub_key,  (unsigned char*)&pub_key.front()  );
    if( priv_key.size() ) BN_bn2bin( bn_priv_key, (unsigned char*)&priv_key.front() );
#else
    pub_key.resize(  BN_num_bytes( dh->pub_key  ) );
    priv_key.resize( BN_num_bytes( dh->priv_key ) );
    if( pub_key.size()  ) BN_bn2bin( dh->pub_key,  (unsigned char*)&pub_key.front()  );
    if( priv_key.size() ) BN_bn2bin( dh->priv_key, (unsigned char*)&priv_key.front() );
#endif
    return true;
}

bool diffie_hellman::compute_shared_key( const char* buf, uint32_t s )
{
    ssl_dh dh(DH_new());
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    auto bn_p        = BN_bin2bn( (unsigned char*)&p.front(),        p.size(),        NULL );
    auto bn_pub_key  = BN_bin2bn( (unsigned char*)&pub_key.front(),  pub_key.size(),  NULL );
    auto bn_priv_key = BN_bin2bn( (unsigned char*)&priv_key.front(), priv_key.size(), NULL );
    auto bn_g        = BN_bin2bn( (unsigned char*)&g, 1, NULL );
    DH_set0_pqg( dh.obj, bn_p, NULL, bn_g );
    DH_set0_key( dh.obj, bn_pub_key, bn_priv_key );
#else
    dh->p        = BN_bin2bn( (unsigned char*)&p.front(),        p.size(),        NULL );
    dh->pub_key  = BN_bin2bn( (unsigned char*)&pub_key.front(),  pub_key.size(),  NULL );
    dh->priv_key = BN_bin2bn( (unsigned char*)&priv_key.front(), priv_key.size(), NULL );
    dh->g        = BN_bin2bn( (unsigned char*)&g, 1, NULL );
#endif
    int check;
    DH_check(dh,&check);
    if( !fc::validate( dh, valid ) )
        return false;

    ssl_bignum pk;
    BN_bin2bn( (unsigned char*)buf, s, pk );
    int est_size = DH_size(dh);
    shared_key.resize( est_size );
    int actual_size = DH_compute_key( (unsigned char*)&shared_key.front(), pk, dh );
    if( actual_size < 0 ) return false;
    if( actual_size != est_size )
        shared_key.resize( actual_size );
    return true;
}

#endif // OPENSSL_VERSION_NUMBER

bool diffie_hellman::compute_shared_key( const std::vector<char>& pubk ) {
    return compute_shared_key( &pubk.front(), pubk.size() );
}

} // namespace fc
