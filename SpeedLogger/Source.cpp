#include <boost/config/compiler/visualc.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <cassert>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <array>
#include <cstdio>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp> 
#include <thread>
#include <fstream>
#include <boost/lexical_cast.hpp>

void progress (bool * stop)
{
    while (!*stop)
    {
        Sleep (300);
        std::cout << "Testing   \r";
        if (*stop)
            break;
        Sleep (300);
        std::cout << "Testing.  \r";
        if (*stop)
            break;
        Sleep (300);
        std::cout << "Testing..  \r";
        if (*stop)
            break;
        Sleep (300);
        std::cout << "Testing... \r";
    }
}

class speed_tester
{
public:
    struct speed
    {
        unsigned long int bytes = 0;
        unsigned long int elapsed = 0;
    };

    struct network_speed
    {
        time_t time;
        speed upload;
        speed download;
    };

    enum class test_type
    {
        upload,
        download
    };

    enum class speed_units
    {
        Bs,
        bs,
        kBs,
        kbs,
        mBs,
        mbs,
        gBs,
        gbs,
    };

    speed_tester (std::string speedtest_cli_path = "speedtest.exe");

    void test ()
    {
        bool stop = false;
        std::thread loader (progress, &stop);

        std::string result = exec ((std::string ("start /b ") + m_speedtest_cli_path + std::string (" -f json-pretty")).c_str ());
        stop = true;
        loader.join ();

        std::stringstream ss;
        ss << result;
        boost::property_tree::ptree json = parse (ss);

        network_speed result_speed;
        result_speed.time = time(nullptr);
        save (json, &result_speed);

        m_history.push_back (result_speed);
    }

    void dump (std::ostream & out)
    {
        for (auto && el : m_history)
        {
            out << "download:\n";
            out << "\telapsed: " << el.download.elapsed << "\n";
            out << "\tbytes: " << el.download.bytes << "\n";

            out << "upload:\n";
            out << "\telapsed: " << el.upload.elapsed << "\n";
            out << "\tbytes: " << el.upload.bytes << "\n";
        }
    }

    template <speed_units unit>
    void dump_readable (std::ostream & out)
    {
        for (auto && el : m_history)
        {
            out << "download: " << convert_speed <unit> (el.download) << "\n";
            out << "upload: " << convert_speed <unit> (el.upload) << "\n";
        }
    }

    template <speed_units unit>
    void dump_last (std::ostream & out)
    {
        out << "download: " << convert_speed <unit> (m_history[m_history.size() - 1].download) << " " << get_unit <unit> () << "\n";
        out << "upload: " << convert_speed <unit> (m_history[m_history.size () - 1].upload) << " " << get_unit <unit> () << "\n";
        out << "time: " << asctime (gmtime (&m_history[m_history.size () - 1].time)) << "\n";
    }

    const std::vector <network_speed> & get_history () const;

    template <speed_units unit>
    void dump_raw_last (std::ostream & out)
    {
        out << convert_speed <unit> (m_history[m_history.size () - 1].download) << " " << convert_speed <unit> (m_history[m_history.size () - 1].upload) << " " << m_history[m_history.size () - 1].time << "\n";
    }

protected:
    std::string exec (const char * cmd);
    boost::property_tree::ptree parse (std::stringstream & ss);
    void print (boost::property_tree::ptree const & pt);
    void save_speed (boost::property_tree::ptree const & pt, network_speed * n_speed, test_type type);
    void save (boost::property_tree::ptree const & pt, network_speed * speed);
    
    template <speed_units unit>
    float convert_speed (speed s);
    

    template <speed_units unit>
    std::string get_unit ()
    {
        switch (unit)
        {
        case speed_tester::speed_units::Bs:
            return "B/s";
            break;
        case speed_tester::speed_units::bs:
            return "b/s";
            break;
        case speed_tester::speed_units::kBs:
            return "KB/s";
            break;
        case speed_tester::speed_units::kbs:
            return "Kb/s";
            break;
        case speed_tester::speed_units::mBs:
            return "MB/s";
            break;
        case speed_tester::speed_units::mbs:
            return "Mb/s";
            break;
        case speed_tester::speed_units::gBs:
            break;
            return "GB/s";
        case speed_tester::speed_units::gbs:
            break;
            return "Gb/s";
        default:
            return "";
            break;
        }
    }

    std::string m_speedtest_cli_path;
    std::vector <network_speed> m_history;
};

int main ()
{
    speed_tester tester;

    std::ofstream fout;
    std::ofstream raw;

    while (true)
    {
        tester.test ();
        tester.dump_last <speed_tester::speed_units::mbs> (std::cout);

        fout.open ("dump.txt", std::ofstream::app);
        tester.dump_last <speed_tester::speed_units::mbs> (fout);
        fout.close ();

        raw.open ("rawdump.txt", std::ofstream::app);
        tester.dump_raw_last <speed_tester::speed_units::mbs> (raw);
        raw.close ();

        Sleep (10 * 60 * 1000);
    }

    return EXIT_SUCCESS;
}

speed_tester::speed_tester (std::string speedtest_cli_path) :
    m_speedtest_cli_path (speedtest_cli_path),
    m_history ()
{

}

const std::vector<speed_tester::network_speed> & speed_tester::get_history () const
{
    return m_history;
}

std::string speed_tester::exec (const char * cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe (_popen (cmd, "r"), _pclose);

    if (!pipe) {
        throw std::runtime_error ("popen() failed!");
    }

    //boost::this_thread::sleep (boost::posix_time::seconds (1000));

    while (fgets (buffer.data (), buffer.size (), pipe.get ()) != nullptr) {
        result += buffer.data ();
    }

    return result;
}

boost::property_tree::ptree speed_tester::parse (std::stringstream & ss)
{
    try
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json (ss, pt);

        return pt;
    }
    catch (std::exception const & e)
    {
        std::cerr << e.what () << std::endl;
    }
}

void speed_tester::print (boost::property_tree::ptree const & pt)
{
    using boost::property_tree::ptree;

    ptree::const_iterator end = pt.end ();

    for (ptree::const_iterator it = pt.begin (); it != end; ++it) {
        std::cout << it->first << ": " << it->second.get_value<std::string> () << std::endl;
        print (it->second);
    }
}

void speed_tester::save (boost::property_tree::ptree const & pt, network_speed * speed)
{
    using boost::property_tree::ptree;

    ptree::const_iterator end = pt.end ();

    for (ptree::const_iterator it = pt.begin (); it != end; ++it)
    {
        if (it->first == std::string ("upload"))
            save_speed (it->second, speed, test_type::upload);
        else if (it->first == std::string ("download"))
            save_speed (it->second, speed, test_type::download);
        else
            save (it->second, speed);
    }
}

void speed_tester::save_speed (boost::property_tree::ptree const & pt, network_speed * n_speed, test_type type)
{
    using boost::property_tree::ptree;

    ptree::const_iterator end = pt.end ();

    speed current;

    for (ptree::const_iterator it = pt.begin (); it != end; ++it)
    {
        if (it->first == std::string ("bytes"))
            current.bytes = it->second.get_value<int> ();
        if (it->first == std::string ("elapsed"))
            current.elapsed = it->second.get_value<int> ();
    }

    switch (type)
    {
    default:
        break;

    case test_type::upload:
        n_speed->upload = current;
        break;

    case test_type::download:
        n_speed->download = current;
        break;
    }


}

template<speed_tester::speed_units unit>
float speed_tester::convert_speed (speed s)
{
    switch (unit)
    {
    defaut:
        return 0.f;
    case speed_units::bs:
        return static_cast<float> (s.bytes) / static_cast<float> (s.elapsed) * 1000 * 8;
    case speed_units::Bs:
        return static_cast<float> (s.bytes) / static_cast<float> (s.elapsed) * 1000;
    case speed_units::kbs:
        return static_cast<float> (s.bytes) / static_cast<float> (s.elapsed) * 1000 / 1024 * 8;
    case speed_units::kBs:
        return static_cast<float> (s.bytes) / static_cast<float> (s.elapsed) * 1000 / 1024 ;
    case speed_units::mbs:
        return static_cast<float> (s.bytes) / static_cast<float> (s.elapsed) * 1000 / 1024 / 1024 * 8;
    case speed_units::mBs:
        return static_cast<float> (s.bytes) / static_cast<float> (s.elapsed) * 1000 / 1024 / 1024;
    case speed_units::gbs:
        return static_cast<float> (s.bytes) / static_cast<float> (s.elapsed) * 1000 / 1024 / 1024 / 1024 * 8;
    case speed_units::gBs:
        return static_cast<float> (s.bytes) / static_cast<float> (s.elapsed) * 1000 / 1024 / 1024 / 1024;
    }
}
