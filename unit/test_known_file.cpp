#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <fstream>
#include <boost/test/unit_test.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>
#include <locale.h>
#include "libed2k/constants.hpp"
#include "libed2k/types.hpp"
#include "libed2k/file.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/log.hpp"

namespace libed2k
{
    namespace aux
    {
        class session_impl_test : public session_impl_base
        {
        public:
            session_impl_test(const session_settings& settings);
            virtual transfer_handle add_transfer(add_transfer_params const&, error_code& ec);
            void stop();
            void wait();
            void run();
            void save();    // this method emulated save_state in session_base

            int                 m_hash_count;
            boost::mutex        m_mutex;
            boost::condition    m_signal;
            std::vector<md4_hash> m_vH;
            std::vector<add_transfer_params> m_vParams;
            bool                m_bAfterSave;
        };

        session_impl_test::session_impl_test(const session_settings& settings) : session_impl_base(settings), m_hash_count(0)
        {
            m_bAfterSave = false;
            m_fmon.start();
            m_vH.push_back(libed2k::md4_hash::fromString("1AA8AFE3018B38D9B4D880D0683CCEB5"));
            m_vH.push_back(libed2k::md4_hash::fromString("E76BADB8F958D7685B4549D874699EE9"));
            m_vH.push_back(libed2k::md4_hash::fromString("49EC2B5DEF507DEA73E106FEDB9697EE"));
            m_vH.push_back(libed2k::md4_hash::fromString("9385DCEF4CB89FD5A4334F5034C28893"));
            m_vH.push_back(libed2k::md4_hash::fromString("9C7F988154D2C9AF16D92661756CF6B2"));
        }

        transfer_handle session_impl_test::add_transfer(add_transfer_params const& t, error_code& ec)
        {
            boost::mutex::scoped_lock lock(m_mutex);

            // after save we load all files from known.met
            if (m_bAfterSave)
            {
                DBG("add transfer after save: " << m_vParams.size());
                ++m_hash_count;
                t.dump();
                BOOST_CHECK(std::find(m_vParams.begin(), m_vParams.end(), t) != m_vParams.end());
                m_vParams.erase(std::remove(m_vParams.begin(), m_vParams.end(), t), m_vParams.end());
                BOOST_CHECK(m_vParams.size() == (5 - m_hash_count));
            }
            else
            {
                // now all files were loaded by file monitor
                ++m_hash_count;

                DBG("add hash for: " << convert_to_native(t.file_path.string()));

                BOOST_CHECK(std::find(m_vH.begin(), m_vH.end(), t.file_hash) != m_vH.end());
                m_vH.erase(std::remove(m_vH.begin(), m_vH.end(), t.file_hash), m_vH.end()); // erase checked item
                BOOST_CHECK(m_vH.size() == (5 - m_hash_count));
                m_vParams.push_back(t);

                if (m_hash_count == 5)
                {
                    m_signal.notify_all();
                }
            }

            return (transfer_handle(boost::weak_ptr<transfer>()));
        }

        void session_impl_test::stop()
        {

            DBG("fmon stop");
            m_fmon.stop();
            DBG("fmon stop complete");
        }

        void session_impl_test::wait()
        {
            boost::mutex::scoped_lock lock(m_mutex);

            if (m_hash_count < 5)
            {
                m_signal.wait(lock);
            }
        }

        void session_impl_test::run()
        {
            m_io_service.run();
        }

        void session_impl_test::save()
        {
            m_hash_count = 0;
            m_bAfterSave = true;
            m_transfers_filenames.clear();
            DBG("session_impl::save_state()");
            known_file_collection kfc;

            std::vector<add_transfer_params>::iterator itr = m_vParams.begin();

            while(itr != m_vParams.end())
            {
                itr->dump();
                kfc.m_known_file_list.add(known_file_entry(itr->file_hash,
                        itr->piece_hash.all_hashes(),
                        itr->file_path,
                        itr->file_size,
                        itr->m_accepted,
                        itr->m_requested,
                        itr->m_transferred,
                        itr->m_priority));

                ++itr;
            }

            try
            {
                if (!m_settings.m_known_file.empty())
                {
                    DBG("save to " << convert_to_native(m_settings.m_known_file));
                    fs::ofstream fstream(convert_to_native(m_settings.m_known_file), std::ios::binary);
                    libed2k::archive::ed2k_oarchive ofa(fstream);
                    ofa << kfc;
                }
            }
            catch(libed2k_exception&)
            {
                BOOST_REQUIRE(false);
            }

        }

    }
}

BOOST_AUTO_TEST_SUITE(test_known_files)

const char chRussianDirectory[] = {'\xEF', '\xBB', '\xBF', '\xD1', '\x80', '\xD1', '\x83', '\xD1', '\x81', '\xD1', '\x81', '\xD0', '\xBA', '\xD0', '\xB0', '\xD1', '\x8F', '\x20', '\xD0', '\xB4', '\xD0', '\xB8', '\xD1', '\x80', '\xD0', '\xB5', '\xD0', '\xBA', '\xD1', '\x82', '\xD0', '\xBE', '\xD1', '\x80', '\xD0', '\xB8', '\xD1', '\x8F', '\x00' };
const char chRussianFilename[] = { '\xD1', '\x80', '\xD1', '\x83', '\xD1', '\x81', '\xD1', '\x81', '\xD0', '\xBA', '\xD0', '\xB8', '\xD0', '\xB9', '\x20', '\xD1', '\x84', '\xD0', '\xB0', '\xD0', '\xB9', '\xD0', '\xBB', '\x00' };


void generate_file(size_t nSize, const char* pchFilename)
{
    std::ofstream of(pchFilename, std::ios_base::binary);

    if (of)
    {
        // generate small file
        for (size_t i = 0; i < nSize; i++)
        {
            of << 'X';
        }
    }
}

struct test_files
{
    test_files() :
                m_file1("./test1.bin"),
                m_file2("./test2.bin"),
                m_file3("./test3.bin"),
                m_file4("./test4.bin"),
                m_file5("./test5.bin")
    {
#ifdef LONG_TIME_TESTS
        // generate test files
        generate_file(100, "./test1.bin");
        generate_file(libed2k::PIECE_SIZE, "./test2.bin");
        generate_file(libed2k::PIECE_SIZE+1, "./test3.bin");
        generate_file(libed2k::PIECE_SIZE*4, "./test4.bin");
        generate_file(libed2k::PIECE_SIZE+4566, "./test5.bin");
#endif
    }

    ~test_files()
    {
        if (libed2k::fs::exists("./test1.bin"))
        {
            libed2k::fs::remove("./test1.bin");
        }

        if (libed2k::fs::exists("./test2.bin"))
        {
            libed2k::fs::remove("./test2.bin");
        }

        if (libed2k::fs::exists("./test3.bin"))
        {
            libed2k::fs::remove("./test3.bin");
        }

        if (libed2k::fs::exists("./test4.bin"))
        {
            libed2k::fs::remove("./test4.bin");
        }

        if (libed2k::fs::exists("./test5.bin"))
        {
            libed2k::fs::remove("./test5.bin");
        }
    }

    libed2k::known_file m_file1;
    libed2k::known_file m_file2;
    libed2k::known_file m_file3;
    libed2k::known_file m_file4;
    libed2k::known_file m_file5;
};

BOOST_FIXTURE_TEST_CASE(test_file_calculate, test_files)
{
#ifdef LONG_TIME_TESTS
    m_file1.init();
    BOOST_CHECK_EQUAL(m_file1.getPiecesCount(), 1);
    BOOST_CHECK_EQUAL(m_file1.getFileHash().toString(), std::string("1AA8AFE3018B38D9B4D880D0683CCEB5"));

    m_file2.init();
    BOOST_CHECK_EQUAL(m_file2.getPiecesCount(), 2);
    BOOST_CHECK_EQUAL(m_file2.getFileHash().toString(), std::string("E76BADB8F958D7685B4549D874699EE9"));

    m_file3.init();
    BOOST_CHECK_EQUAL(m_file3.getPiecesCount(), 2);
    BOOST_CHECK_EQUAL(m_file3.getFileHash().toString(), std::string("49EC2B5DEF507DEA73E106FEDB9697EE"));


    m_file4.init();
    BOOST_CHECK_EQUAL(m_file4.getPiecesCount(), 5);
    BOOST_CHECK_EQUAL(m_file4.getFileHash().toString(), std::string("9385DCEF4CB89FD5A4334F5034C28893"));

    m_file5.init();
    BOOST_CHECK_EQUAL(m_file5.getPiecesCount(), 2);
    BOOST_CHECK_EQUAL(m_file5.getFileHash().toString(), std::string("9C7F988154D2C9AF16D92661756CF6B2"));
#endif
}

void create_directory_tree()
{
    std::string strDirectory = chRussianDirectory;
    std::string strFilename  = chRussianFilename;
    libed2k::fs::path p(libed2k::convert_to_native(libed2k::bom_filter(strDirectory)));
    libed2k::fs::create_directories(p);
    p /= libed2k::convert_to_native(libed2k::bom_filter(strDirectory));
    libed2k::fs::create_directories(p);

    //p /= libed2k::convert_to_native(libed2k::bom_filter(strFilename));

    libed2k::fs::path p1 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "01.txt"));
    libed2k::fs::path p2 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "02.txt"));
    libed2k::fs::path p3 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "03.txt"));
    libed2k::fs::path p4 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "04.txt"));
    libed2k::fs::path p5 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "05.txt"));

    generate_file(100, p1.string().c_str());
    generate_file(libed2k::PIECE_SIZE, p2.string().c_str());
    generate_file(libed2k::PIECE_SIZE+1, p3.string().c_str());
    generate_file(libed2k::PIECE_SIZE*4, p4.string().c_str());
    generate_file(libed2k::PIECE_SIZE+4566, p5.string().c_str());

    std::ofstream fstr(p.string().c_str());

}

void drop_directory_tree()
{
    std::string strDirectory = chRussianDirectory;
    libed2k::fs::path p(libed2k::convert_to_native(libed2k::bom_filter(strDirectory)));
    libed2k::fs::remove_all(p);
}

BOOST_AUTO_TEST_CASE(test_string_conversions)
{
    setlocale(LC_CTYPE, "");
    std::string strDirectory = chRussianDirectory;
    std::string strNative = libed2k::convert_to_native(libed2k::bom_filter(strDirectory));

    if (CHECK_BOM(strDirectory.size(), strDirectory))
    {
        BOOST_CHECK_EQUAL(strDirectory.substr(3), libed2k::convert_from_native(strNative));
    }
}

BOOST_AUTO_TEST_CASE(test_file_monitor)
{
    LOGGER_INIT()
    libed2k::session_settings s;
    s.m_fd_list.push_back(std::make_pair(std::string(chRussianDirectory), true));
    libed2k::aux::session_impl_test st(s);
    st.stop();
}

BOOST_AUTO_TEST_CASE(test_session)
{
    //LOGGER_INIT()
    setlocale(LC_CTYPE, "");

    libed2k::session_settings s;

    s.m_known_file = "known.met";
    s.m_fd_list.push_back(std::make_pair(std::string(chRussianDirectory), true));

    create_directory_tree();

    libed2k::aux::session_impl_test st(s);
    st.load_state();

    while(st.m_hash_count < 5)
    {
        st.m_io_service.run_one();
        st.m_io_service.reset();
    }

    st.wait();
    st.stop();

    // file monitor was stopped
    st.save();
    st.load_state();
    BOOST_CHECK_EQUAL(st.m_hash_count, 5);


    drop_directory_tree();
    libed2k::fs::path p = "known.met";
    libed2k::fs::remove(p);
}

BOOST_AUTO_TEST_SUITE_END()