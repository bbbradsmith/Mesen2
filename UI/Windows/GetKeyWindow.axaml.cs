using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Avalonia.Interactivity;
using System;
using Mesen.Config.Shortcuts;
using System.Collections.Generic;
using System.Linq;
using Avalonia.Input;
using System.ComponentModel;
using Mesen.Interop;
using Avalonia.Threading;
using Avalonia;
using Mesen.Config;
using Mesen.Utilities;
using Mesen.Localization;
using Avalonia.Remote.Protocol.Input;
using DynamicData;
using Avalonia.Layout;

namespace Mesen.Windows
{
	public class GetKeyWindow : MesenWindow
	{
		private DispatcherTimer _timer; 
		
		private List<UInt16> _prevScanCodes = new List<UInt16>();
		private TextBlock lblCurrentKey;
		private bool _allowKeyboardOnly;
		private List<ushort> _momentaryCodes = new List<ushort>();
		private List<bool> _momentaryHit = new List<bool>();

		private static readonly string[] MomentaryNames = {
			"Print Screen",
			"Pause"
		};

		public string HintLabel { get; }
		public bool SingleKeyMode { get; set; } = false;
		
		public DbgShortKeys DbgShortcutKey { get; set; } = new DbgShortKeys();
		public KeyCombination ShortcutKey { get; set; } = new KeyCombination();

		[Obsolete("For designer only")]
		public GetKeyWindow() : this(false) { }

		public GetKeyWindow(bool allowKeyboardOnly)
		{
			_allowKeyboardOnly = allowKeyboardOnly;
			HintLabel = ResourceHelper.GetMessage(_allowKeyboardOnly ? "SetKeyHint" : "SetKeyMouseHint");

			//Required for keyboard input to work properly in Linux/macOS
			this.Focusable = true;

			_momentaryCodes = new List<ushort>(MomentaryNames.Length);
			_momentaryHit = new List<bool>(MomentaryNames.Length);
			foreach(string name in MomentaryNames) {
				_momentaryCodes.Add(InputApi.GetKeyCode(name));
				_momentaryHit.Add(false);
			}

			InitializeComponent();

			lblCurrentKey = this.GetControl<TextBlock>("lblCurrentKey");
			
			_timer = new DispatcherTimer(TimeSpan.FromMilliseconds(25), DispatcherPriority.Normal, (s, e) => UpdateKeyDisplay());
			_timer.Start();

			//Allow us to catch LeftAlt/RightAlt key presses
			this.AddHandler(InputElement.KeyDownEvent, OnPreviewKeyDown, RoutingStrategies.Tunnel, true);
			this.AddHandler(InputElement.KeyUpEvent, OnPreviewKeyUp, RoutingStrategies.Tunnel, true);
		}

		protected override void OnClosed(EventArgs e)
		{
			_timer?.Stop();
			base.OnClosed(e);
		}

		private void InitializeComponent()
		{
			AvaloniaXamlLoader.Load(this);
		}

		private void OnPreviewKeyDown(object? sender, KeyEventArgs e)
		{
			//System.Diagnostics.Debug.WriteLine("KeyDown: "+e.Key.ToString());

			InputApi.SetKeyState((UInt16)e.Key, true);
			DbgShortcutKey = new DbgShortKeys(e.KeyModifiers, e.Key);
			
			// Track momentary keys which may have immediate KeyUp following
			int m = _momentaryCodes.IndexOf((UInt16)e.Key);
			if(m >= 0) {
				_momentaryHit[m] = true;
			}

			e.Handled = true;
		}

		private void OnPreviewKeyUp(object? sender, KeyEventArgs e)
		{
			//System.Diagnostics.Debug.WriteLine("KeyUp: "+e.Key.ToString());

			InputApi.SetKeyState((UInt16)e.Key, false);

			// Track momentary keys which may have KeyUp without KeyDown
			int m = _momentaryCodes.IndexOf((UInt16)e.Key);
			if(m >= 0) {
				_momentaryHit[m] = true;
			}

			if(_allowKeyboardOnly) {
				this.Close();
			}
			e.Handled = true;
		}

		protected override void OnOpened(EventArgs e)
		{
			base.OnOpened(e);

			lblCurrentKey.IsVisible = !this.SingleKeyMode;
			lblCurrentKey.Height = this.SingleKeyMode ? 0 : 40;

			ShortcutKey = new KeyCombination();
			InputApi.UpdateInputDevices();
			InputApi.ResetKeyState();

			//Prevent other keybindings from interfering/activating
			InputApi.DisableAllKeys(true);
		}

		protected override void OnClosing(WindowClosingEventArgs e)
		{
			base.OnClosing(e);
			InputApi.DisableAllKeys(false);
		}

		private void SelectKeyCombination(KeyCombination key)
		{
			if(!string.IsNullOrWhiteSpace(key.ToString())) {
				ShortcutKey = key;
				this.Close();
			}
		}

		private void UpdateKeyDisplay()
		{
			if(!_allowKeyboardOnly) {
				SystemMouseState mouseState = InputApi.GetSystemMouseState(IntPtr.Zero);
				PixelPoint mousePos = new PixelPoint(mouseState.XPosition, mouseState.YPosition);
				PixelRect clientBounds = new PixelRect(this.PointToScreen(new Point(0, 0)), PixelSize.FromSize(Bounds.Size, LayoutHelper.GetLayoutScale(this) / InputApi.GetPixelScale()));
				bool mouseInsideWindow = clientBounds.Contains(mousePos);
				InputApi.SetKeyState(MouseManager.LeftMouseButtonKeyCode, mouseInsideWindow && mouseState.LeftButton);
				InputApi.SetKeyState(MouseManager.RightMouseButtonKeyCode, mouseInsideWindow && mouseState.RightButton);
				InputApi.SetKeyState(MouseManager.MiddleMouseButtonKeyCode, mouseInsideWindow && mouseState.MiddleButton);
				InputApi.SetKeyState(MouseManager.MouseButton4KeyCode, mouseInsideWindow && mouseState.Button4);
				InputApi.SetKeyState(MouseManager.MouseButton5KeyCode, mouseInsideWindow && mouseState.Button5);

				List<UInt16> scanCodes = InputApi.GetPressedKeys();

				// Momentary keys must persist for at least one update
				for(ushort i = 0; i < _momentaryHit.Count; i++) {
					if(_momentaryHit[i]) {
						ushort code = _momentaryCodes[i];
						if (!scanCodes.Contains(code)) {
							scanCodes.Add(code);
						}
						_momentaryHit[i] = false;
					}
				}

				if(this.SingleKeyMode) {
					if(scanCodes.Count >= 1) {
						//Always use the largest scancode (when multiple buttons are pressed at once)
						scanCodes = new List<UInt16> { scanCodes.OrderBy(code => -code).First() };
						this.SelectKeyCombination(new KeyCombination(scanCodes));
					}
				} else {
					KeyCombination key = new KeyCombination(_prevScanCodes);
					this.GetControl<TextBlock>("lblCurrentKey").Text = key.ToString();

					if(scanCodes.Count < _prevScanCodes.Count) {
						//Confirm key selection when the user releases a key
						this.SelectKeyCombination(key);
					}

					_prevScanCodes = scanCodes;
				}
			} else {
				this.GetControl<TextBlock>("lblCurrentKey").Text = DbgShortcutKey.ToString();
			}
		}
	}
}
