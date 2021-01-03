//#pragma once
//#include <string>
//#include <functional>
//#include <utility>


//namespace sqf
//{
//
//    class commandhost
//    {
//    public:
//        class path_base
//        {
//        public:
//            virtual sqf::value execute(sqf::value* params, size_t params_count) = 0;
//        };
//    public:
//        struct in
//        {
//            sqf::value value;
//            in(sqf::value v) : value(v)
//            {}
//            operator bool() const { return (bool)value; }
//            operator float() const { return (float)value; }
//            operator std::string() const { return (std::string)value; }
//            operator std::vector<sqf::value>() const { return (std::vector<sqf::value>)value; }
//        };
//
//        template<typename ... TPack> class path : public path_base
//        {
//        private:
//            using fnc = std::function<sqf::value(TPack...)>;
//            constexpr static size_t pack_length()
//            {
//                return sizeof...(TPack);
//            }
//            fnc m_func;
//        public:
//            path(fnc func) : m_func(func)
//            {
//
//            }
//
//            virtual sqf::value execute(sqf::value* params, size_t params_count) override
//            {
//                if (params_count != pack_length()) { return {}; }
//                return execute_do<pack_length()>(params, {});
//            }
//
//            template<size_t... seq>
//            sqf::value execute_do(sqf::value* params, std::integer_sequence<size_t, seq...>) const
//            {
//                return execute_do_actual(params[seq]...);
//            }
//            template<class t, class ... pack>
//            sqf::value execute_do_actual(t data, pack... p) const
//            {
//                return m_func(in(data), p...);
//            }
//        };
//    };
//}