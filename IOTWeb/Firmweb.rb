require 'sinatra'

configure do
	set :port, 1884
end

set :bind, '0.0.0.0'

# names of the fields in an OTA request
req_field_names = %w{ device firmware version_current version_desired }

get '/' do
	logger.info("route /")
	"Hello World!"
end

get '/custom_ota' do
	logger.info("route custom ota")

	# First, set up a hash, reqfields, indexed by the field names (above)
	tmp = Array.new
	request.env['HTTP_X_ESP8266_VERSION'].each_line('=') {|f| tmp << f.chomp('=')}
	reqfields = Hash.new
	tmp.each_index {|i| reqfields[req_field_names[i]] = tmp[i]}

	# Now, find the firmware file
	#xxx firmware_file = 'FIRMWARE/hello-world/0.2.2'
	firmware_file = "FIRMWARE/#{reqfields['firmware']}/#{reqfields['version_desired']}"
	logger.info("Sending file " + firmware_file)
	#send_file firmware_file
end
