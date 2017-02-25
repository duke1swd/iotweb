require 'sinatra'

configure do
	set :port, 1884
end

set :bind, '0.0.0.0'

# names of the fields in an OTA request
req_field_names = %w{ device firmware version_current version_desired }

# Used for handling errors during OTA processing
class OtaError < Exception
  def initialize(msg)
    @msg = msg
  end

  def to_S
    @msg
  end
end


# Routes go here


get '/' do
	logger.info("route /")
	"Hello World!"
end

get '/custom_ota' do
	logger.info("route custom ota")

    begin
	# First, set up a hash, reqfields, indexed by the field names (above)
	reqfields = Hash.new
	begin
	    tmp = Array.new
	    request.env['HTTP_X_ESP8266_VERSION'].each_line('=') {|f| tmp << CGI.escape(f.chomp('='))}
	    tmp.each_index {|i| reqfields[req_field_names[i]] = tmp[i]}
	rescue Exception
	    raise OtaError.new("Missing or unparsable HTTP_X_ESP8266_VERSION header")
	end

	# Now, find the firmware file
	firmware_file = "FIRMWARE/#{reqfields['firmware']}/#{reqfields['version_desired']}"
	raise OtaError.new("Requested file #{firmware_file} does not exist") unless File.exist?(firmware_file)
	logger.info("Sending file " + firmware_file)
	#send_file firmware_file
    
    rescue OtaError
    	logger.info("OTA Error: #{$!.to_S}");
    end
end
