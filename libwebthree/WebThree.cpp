/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "WebThree.h"

#include <libethashseal/Ethash.h>
#include <libethashseal/EthashClient.h>
#include <libethereum/ClientTest.h>
#include <libethereum/Defaults.h>

#include <skale/buildinfo.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::shh;

static_assert( BOOST_VERSION >= 106400, "Wrong boost headers version" );

WebThreeDirect::WebThreeDirect( std::string const& _clientVersion,
    boost::filesystem::path const& _dbPath, boost::filesystem::path const& _snapshotPath,
    eth::ChainParams const& _params, WithExisting _we, std::set< std::string > const& _interfaces,
    bool _testing ) try : m_clientVersion( _clientVersion ) {
    if ( _dbPath.size() )
        Defaults::setDBPath( _dbPath );
    if ( _interfaces.count( "eth" ) ) {
        Ethash::init();
        NoProof::init();

        auto tq_limits = TransactionQueue::Limits{100000, 1024};

        if ( _testing ) {
            m_ethereum.reset( new eth::ClientTest(
                _params, ( int ) _params.networkID, shared_ptr< GasPricer >(), _dbPath, _we ) );
            m_ethereum->injectSkaleHost();
        } else {
            if ( _params.sealEngineName == Ethash::name() ) {
                m_ethereum.reset( new eth::EthashClient( _params, ( int ) _params.networkID,
                    shared_ptr< GasPricer >(), _dbPath, _snapshotPath, _we, tq_limits ) );
                m_ethereum->injectSkaleHost();
            } else if ( _params.sealEngineName == NoProof::name() ) {
                m_ethereum.reset( new eth::Client( _params, ( int ) _params.networkID,
                    shared_ptr< GasPricer >(), _dbPath, _snapshotPath, _we, tq_limits ) );
                m_ethereum->injectSkaleHost();
            } else
                BOOST_THROW_EXCEPTION( ChainParamsInvalid() << errinfo_comment(
                                           "Unknown seal engine: " + _params.sealEngineName ) );
        }
        m_ethereum->startWorking();

        const auto* buildinfo = skale_get_buildinfo();
        m_ethereum->setExtraData(
            rlpList( 0, string{buildinfo->project_version}.substr( 0, 5 ) + "++" +
                            string{buildinfo->git_commit_hash}.substr( 0, 4 ) +
                            string{buildinfo->build_type}.substr( 0, 1 ) +
                            string{buildinfo->system_name}.substr( 0, 5 ) +
                            string{buildinfo->compiler_id}.substr( 0, 3 ) ) );
    }
} catch ( const std::exception& ) {
    std::throw_with_nested( CreationException() );
}

WebThreeDirect::~WebThreeDirect() {
    // Utterly horrible right now - WebThree owns everything (good), but:
    // m_net (Host) owns the eth::EthereumHost via a shared_ptr.
    // The eth::EthereumHost depends on eth::Client (it maintains a reference to the BlockChain
    // field of Client). eth::Client (owned by us via a unique_ptr) uses eth::EthereumHost (via a
    // weak_ptr). Really need to work out a clean way of organising ownership and guaranteeing
    // startup/shutdown is perfect.

    // Have to call stop here to get the Host to kill its io_service otherwise we end up with
    // left-over reads, still referencing Sessions getting deleted *after* m_ethereum is reset,
    // causing bad things to happen, since the guarantee is that m_ethereum is only reset *after*
    // all sessions have ended (sessions are allowed to use bits of data owned by m_ethereum).
}

std::string WebThreeDirect::composeClientVersion( std::string const& _client ) {
    const auto* buildinfo = skale_get_buildinfo();
    return _client + "/" + buildinfo->project_version + "/" + buildinfo->system_name + "/" +
           buildinfo->compiler_id + buildinfo->compiler_version + "/" + buildinfo->build_type;
}
