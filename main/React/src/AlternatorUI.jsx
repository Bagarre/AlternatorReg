import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";

function ConfigForm() {
  const [config, setConfig] = useState({
    targetVoltage: '14.4',
    currentLimit: '100',
    floatVoltage: '13.6',
    derateTemp: '82',
    canInput: true,
  });

  const handleConfigChange = (e) => {
    const { name, value, type, checked } = e.target;
    setConfig({
      ...config,
      [name]: type === "checkbox" ? checked : value,
    });
  };

  const defaults = {
    targetVoltage: '14.4',
    currentLimit: '100',
    floatVoltage: '13.6',
    derateTemp: '82',
    canInput: 'true',
  };

  return (
    <>
      {Object.entries(config).map(([key, value]) => (
        <div
          key={key}
          className="grid grid-cols-3 items-center gap-3"
        >
          <label className="text-sm capitalize" htmlFor={key}>
            {key.replace(/([A-Z])/g, ' $1')}
          </label>
          <Input
            id={key}
            name={key}
            className="col-span-1"
            type={
              typeof value === "boolean"
                ? "checkbox"
                : 'text'
            }
            checked={typeof value === "boolean" ? value : undefined}
            value={typeof value === "boolean" ? undefined : value}
            onChange={handleConfigChange}
          />
          <span className="text-xs text-gray-400 col-span-1">
            {defaults[key] ?? ''}
          </span>
        </div>
      ))}
      <div className="flex justify-end pt-2">
        <Button className="bg-green-600 hover:bg-green-700 text-white px-4">
          Save Config
        </Button>
      </div>
    </>
  );
}

export default function AlternatorUI() {
  const [isDisabled, setIsDisabled] = useState(false);
  const statusRows = [
    ['Voltage', '14.3 V', 'text-blue-600'],
    ['Current', '83.5 A', 'text-orange-600'],
    ['Field PWM', '62%', ''],
    ['Alt Temp', '161°F', ''],
    ['Batt Temp', '85°F', ''],
    ['Engine RPM', '1450 RPM', ''],
    ['Stage', 'Bulk', ''],
    ['CAN Status', 'OK', ''],
    ['Last CAN Update', '12:45:12', ''],
    ['BMS Permission', 'Yes', ''],
  ];

  return (
    <div className="flex flex-col min-h-screen bg-gradient-to-b from-gray-100 to-white text-gray-800">
      <header className={\`text-white p-4 text-center text-xl font-bold shadow sticky top-0 z-10 \${isDisabled ? 'bg-blue-600' : 'bg-gray-500'}\`}>
        Alternator Control
      </header>

      <main className="flex-1 p-4 space-y-6 max-w-md mx-auto">
        <div className="bg-white rounded-xl shadow p-4">
          <h2 className="text-lg font-semibold mb-4">Status</h2>
          <div className="grid grid-cols-2">
            {statusRows.map(([label, value, color], i) => (
              <div key={label} className={\`contents\`}>
                <div className={\`text-sm px-2 py-1 \${i % 2 === 0 ? 'bg-white' : 'bg-gray-100'}\`}>{label}</div>
                <div className={\`text-right font-semibold px-2 py-1 \${color} \${i % 2 === 0 ? 'bg-white' : 'bg-gray-100'}\`}>{value}</div>
              </div>
            ))}
          </div>
          <div className="flex justify-end pt-4 gap-2">
            <span className="text-sm font-medium text-gray-700 self-center">
              {isDisabled ? 'Enabled' : 'Disabled'}
            </span>
            <label className="relative inline-flex items-center cursor-pointer w-16 h-8">
              <input
                type="checkbox"
                className="sr-only peer"
                checked={isDisabled}
                onChange={() => setIsDisabled(!isDisabled)}
              />
              <div className="w-full h-full bg-gray-300 rounded-full peer peer-checked:bg-green-500 transition-all"></div>
              <div className="absolute left-1 top-1 bg-white w-6 h-6 rounded-full shadow-md transform peer-checked:translate-x-8 transition-transform"></div>
            </label>
            <Button className="bg-blue-600 hover:bg-blue-700 text-white px-4">Reboot</Button>
          </div>
        </div>

        <div className="bg-white rounded-xl shadow p-4">
          <h2 className="text-lg font-semibold mb-4">Network</h2>
          <div className="space-y-3">
            {['wifiSSID', 'wifiPassword'].map((key) => (
              <div key={key} className="grid grid-cols-3 items-start gap-3">
                <label className="text-sm capitalize" htmlFor={key}>
                  {key.replace(/([A-Z])/g, ' $1')}
                </label>
                <Input
                  id={key}
                  name={key}
                  className="col-span-2"
                  type={key.toLowerCase().includes('password') ? 'password' : 'text'}
                />
              </div>
            ))}
            <div className="flex justify-end pt-2 col-span-3 gap-2">
              <Button className="bg-blue-600 hover:bg-blue-700 text-white px-4">
                Submit
              </Button>
              <Button className="bg-gray-600 hover:bg-gray-700 text-white px-4">
                Reset Network
              </Button>
            </div>
          </div>
        </div>

        <div className="bg-white rounded-xl shadow p-4">
          <h2 className="text-lg font-semibold mb-4">Configuration</h2>
          <ConfigForm />
        </div>
      </main>

      <footer className="text-xs text-center text-gray-500 p-2 mt-auto sticky bottom-0 bg-white border-t">
        Author: David Ross · License: MIT
      </footer>
    </div>
  );
}