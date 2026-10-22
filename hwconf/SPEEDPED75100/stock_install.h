/* SPDX-License-Identifier: GPL-3.0-or-later */
// Marker is committed last. Interrupted installation retries before app startup.
bool speedped_install_stock_once(void) {
    if (speedped_stock_installed()) return true;
    if (!speedped_stock_write_mc()) return false;
    if (!speedped_stock_write_app()) return false;
    if (!speedped_stock_write_bike()) return false;
    return speedped_stock_mark_installed();
}
