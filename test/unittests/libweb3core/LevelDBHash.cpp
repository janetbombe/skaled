#include <libdevcore/LevelDB.h>
#include <libdevcore/TransientDirectory.h>
#include <boost/test/unit_test.hpp>

#include <string>

BOOST_AUTO_TEST_SUITE( LevelDBHashBase )

BOOST_AUTO_TEST_CASE( hash ) {
    dev::TransientDirectory td;

    std::vector< std::pair< std::string, std::string > > randomKeysValues(123);

    dev::h256 hash;
    {
        std::unique_ptr< dev::db::LevelDB > db( new dev::db::LevelDB( td.path() ) );
        BOOST_REQUIRE( db );

        db->insert( dev::db::Slice( "PieceUsageBytes" ), dev::db::Slice( "123456789" ) );
        db->insert( dev::db::Slice( "ppieceUsageBytes" ), dev::db::Slice( "123456789" ) );

        for ( size_t i = 0; i < 123; ++i ) {
            std::string key = dev::h256::random().hex();
            std::string value = dev::h256::random().hex();
            db->insert( dev::db::Slice(key), dev::db::Slice(value) );

            randomKeysValues[i] = { key, value };
        }

        hash = db->hashBase();
    }

    boost::filesystem::remove_all( td.path() );
    BOOST_REQUIRE( !boost::filesystem::exists( td.path() ) );

    dev::h256 hash_same;
    {
        std::unique_ptr< dev::db::LevelDB > db_copy( new dev::db::LevelDB( td.path() ) );
        BOOST_REQUIRE( db_copy );

        for ( size_t i = 0; i < 123; ++i ) {
            std::string key = randomKeysValues[i].first;
            std::string value = randomKeysValues[i].second;
            db_copy->insert( dev::db::Slice(key), dev::db::Slice(value) );
        }
        db_copy->insert( dev::db::Slice( "PieceUsageBytes" ), dev::db::Slice( "123456789" ) );
        db_copy->insert( dev::db::Slice( "ppieceUsageBytes" ), dev::db::Slice( "123456789" ) );

        hash_same = db_copy->hashBase();
    }

    BOOST_REQUIRE( hash == hash_same );

    boost::filesystem::remove_all( td.path() );
    BOOST_REQUIRE( !boost::filesystem::exists( td.path() ) );

    dev::h256 hashPartially;
    {
        {
            std::unique_ptr< dev::db::LevelDB > db_copy( new dev::db::LevelDB( td.path() ) );
            BOOST_REQUIRE( db_copy );

            for ( size_t i = 0; i < 123; ++i ) {
                std::string key = randomKeysValues[i].first;
                std::string value = randomKeysValues[i].second;
                db_copy->insert( dev::db::Slice(key), dev::db::Slice(value) );
            }

            db_copy->insert( dev::db::Slice( "PieceUsageBytes" ), dev::db::Slice( "123456789" ) );
            db_copy->insert( dev::db::Slice( "ppieceUsageBytes" ), dev::db::Slice( "123456789" ) );
        }

        std::array< std::string, 11 > lexographicKeysSegments = { "0", "2", "4", "6", "8", "A",
                                                                  "F", "a", "c", "e", "{" };

        secp256k1_sha256_t dbCtx;
        secp256k1_sha256_initialize( &dbCtx );

        for (size_t i = 0; i < lexographicKeysSegments.size() - 1; ++i) {
            std::unique_ptr< dev::db::LevelDB > db( new dev::db::LevelDB( td.path(),
                dev::db::LevelDB::defaultSnapshotReadOptions(), dev::db::LevelDB::defaultWriteOptions(),
                dev::db::LevelDB::defaultSnapshotDBOptions() ) );

            db->hashBasePartially( &dbCtx, lexographicKeysSegments[i], lexographicKeysSegments[i + 1] );
        }

        secp256k1_sha256_finalize( &dbCtx, hashPartially.data() );
    }

    BOOST_REQUIRE( hash == hashPartially );

    boost::filesystem::remove_all( td.path() );
    BOOST_REQUIRE( !boost::filesystem::exists( td.path() ) );

    dev::h256 hash_diff;
    {
        std::unique_ptr< dev::db::LevelDB > db_diff( new dev::db::LevelDB( td.path() ) );
        BOOST_REQUIRE( db_diff );

        for ( size_t i = 0; i < 123; ++i ) {
            std::string key = dev::h256::random().hex();
            std::string value = dev::h256::random().hex();
            db_diff->insert( dev::db::Slice(key), dev::db::Slice(value) );
        }

        hash_diff = db_diff->hashBase();
    }

    BOOST_REQUIRE( hash != hash_diff );
}

BOOST_AUTO_TEST_SUITE_END()
