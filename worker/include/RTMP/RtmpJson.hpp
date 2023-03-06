#ifndef MS_RTMP_JSON_HPP
#define MS_RTMP_JSON_HPP
#include <string>
#include <vector>

namespace RTMP
{

	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	// JSON decode
	// 1. RtmpJsonAny: read any from str:char*
	//        RtmpJsonAny* any = NULL;
	//        if ((any = RtmpJsonAny::loads(str)) == NULL) {
	//            return -1;
	//         }
	//        srs_assert(pany); // if success, always valid object.
	// 2. RtmpJsonAny: convert to specifid type, for instance, string
	//        RtmpJsonAny* any = ...
	//        if (any->is_string()) {
	//            string v = any->to_str();
	//        }
	//
	// For detail usage, see interfaces of each object.
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	// @see: https://github.com/udp/json-parser

	class RtmpAmf0Any;
	class RtmpJsonArray;
	class RtmpJsonObject;

	class RtmpJsonAny
	{
	public:
		char marker;
		// Don't directly create this object,
		// please use RtmpJsonAny::str() to create a concreated one.
	protected:
		RtmpJsonAny();

	public:
		virtual ~RtmpJsonAny();

	public:
		virtual bool is_string();
		virtual bool is_boolean();
		virtual bool is_integer();
		virtual bool is_number();
		virtual bool is_object();
		virtual bool is_array();
		virtual bool is_null();

	public:
		// Get the string of any when is_string() indicates true.
		// user must ensure the type is a string, or assert failed.
		virtual std::string to_str();
		// Get the boolean of any when is_boolean() indicates true.
		// user must ensure the type is a boolean, or assert failed.
		virtual bool to_boolean();
		// Get the integer of any when is_integer() indicates true.
		// user must ensure the type is a integer, or assert failed.
		virtual int64_t to_integer();
		// Get the number of any when is_number() indicates true.
		// user must ensure the type is a number, or assert failed.
		virtual double to_number();
		// Get the object of any when is_object() indicates true.
		// user must ensure the type is a object, or assert failed.
		virtual RtmpJsonObject* to_object();
		// Get the ecma array of any when is_ecma_array() indicates true.
		// user must ensure the type is a ecma array, or assert failed.
		virtual RtmpJsonArray* to_array();

	public:
		virtual std::string dumps();
		virtual RtmpAmf0Any* to_amf0();

	public:
		static RtmpJsonAny* str(const char* value = NULL);
		static RtmpJsonAny* str(const char* value, int length);
		static RtmpJsonAny* boolean(bool value = false);
		static RtmpJsonAny* integer(int64_t value = 0);
		static RtmpJsonAny* number(double value = 0.0);
		static RtmpJsonAny* null();
		static RtmpJsonObject* object();
		static RtmpJsonArray* array();

	public:
		// Read json tree from string.
		// @return json object. NULL if error.
		static RtmpJsonAny* loads(std::string str);
	};

	class RtmpJsonObject : public RtmpJsonAny
	{
	private:
		typedef std::pair<std::string, RtmpJsonAny*> RtmpJsonObjectPropertyType;
		std::vector<RtmpJsonObjectPropertyType> properties;

	private:
		// Use RtmpJsonAny::object() to create it.
		friend class RtmpJsonAny;
		RtmpJsonObject();

	public:
		virtual ~RtmpJsonObject();

	public:
		virtual int count();
		// @remark: max index is count().
		virtual std::string key_at(int index);
		// @remark: max index is count().
		virtual RtmpJsonAny* value_at(int index);

	public:
		virtual std::string dumps();
		virtual RtmpAmf0Any* to_amf0();

	public:
		virtual RtmpJsonObject* set(std::string key, RtmpJsonAny* value);
		virtual RtmpJsonAny* get_property(std::string name);
		virtual RtmpJsonAny* ensure_property_string(std::string name);
		virtual RtmpJsonAny* ensure_property_integer(std::string name);
		virtual RtmpJsonAny* ensure_property_number(std::string name);
		virtual RtmpJsonAny* ensure_property_boolean(std::string name);
		virtual RtmpJsonAny* ensure_property_object(std::string name);
		virtual RtmpJsonAny* ensure_property_array(std::string name);
	};

	class RtmpJsonArray : public RtmpJsonAny
	{
	private:
		std::vector<RtmpJsonAny*> properties;

	private:
		// Use RtmpJsonAny::array() to create it.
		friend class RtmpJsonAny;
		RtmpJsonArray();

	public:
		virtual ~RtmpJsonArray();

	public:
		virtual int count();
		// @remark: max index is count().
		virtual RtmpJsonAny* at(int index);
		virtual RtmpJsonArray* add(RtmpJsonAny* value);
		// alias to add.
		virtual RtmpJsonArray* append(RtmpJsonAny* value);

	public:
		virtual std::string dumps();
		virtual RtmpAmf0Any* to_amf0();
	};

	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	// JSON encode, please use JSON.dumps() to encode json object.
} // namespace RTMP

#endif